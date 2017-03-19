/*
 * dang_san.c
 * Design is based on per-Thread log model.
 */
#include "metadata.h"
#include "dang_san.h"
#include "dsan_common.h"
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>

#ifdef DANG_STATS
#include "dsan_stats.h"
#define STATS(x) x
#else
#define STATS(x)
#define STATS_ALLOC(name, size)
#define STATS_FREE(name, size)
#endif

#include "dsan_atomics.h"
static unsigned long threadglobal_id = 0;
static __thread unsigned long threadlocal_id = 0;       /* TODO: Check attribute initial-exec */
#define DANG_GET_THREADID(log_id) log_id = threadlocal_id;
#define DSAN_ATOMIC_CMPXCHNG_ONCE(ptr, old_value, new_value) \
            dang_atomic_cmpxchng_once(ptr, old_value, new_value)


#define unlikely(x)     __builtin_expect((x),0)

__thread bool malloc_flag;
__thread bool free_flag;
__thread int dang_ignore_free;

extern __thread void *tcmalloc_stackptr;

/* Declare extern stack/global start and end */
extern unsigned long dang_global_start;
extern unsigned long dang_global_size;
extern __thread unsigned long dang_stack_start;
extern __thread unsigned long dang_stack_size;

/*
 * Shamelessly using per-thread
 * exception handling code snippet by : Drew Eckhardt
 */
struct thread_siginfo {
    sigjmp_buf jmp;
    volatile int canjmp;
};

static __thread struct thread_siginfo thread_state;

static void
dang_segv_handler(int sig, siginfo_t *info, void *where) {

    if (!thread_state.canjmp) {
	/* another segfault for the debugger */
	struct sigaction sa = { .sa_handler = SIG_DFL };
	sigaction(SIGSEGV, &sa, NULL);
	return;
    }

    STATS(dang_stats.segfaults++);

    thread_state.canjmp = 0;
    siglongjmp(thread_state.jmp, 1);
}

static __attribute__((always_inline)) void dang_sighandler_set(
    struct sigaction *sa_old) {
    struct sigaction sa_new = {
	.sa_sigaction = dang_segv_handler,
    };
    sigaction(SIGSEGV, &sa_new, sa_old);
}

static __attribute__((always_inline)) void dang_sighandler_unset(
    const struct sigaction *sa_old) {
    sigaction(SIGSEGV, sa_old, NULL);
}

static __attribute__((always_inline)) unsigned long *dang_compress_add2(
	unsigned long *value, unsigned long *ptr_addr) {
	unsigned long v = (unsigned long) value;
	unsigned long p = (unsigned long) ptr_addr;
	return (unsigned long *) (v | 0x0001000000000000UL | ((p & 0xfe) << 48));
}
	
static __attribute__((always_inline)) unsigned long *dang_compress_add3(
	unsigned long *value, unsigned long *ptr_addr) {
	unsigned long v = (unsigned long) value;
	unsigned long p = (unsigned long) ptr_addr;
	return (unsigned long *) (v | 0x0100000000000000UL | ((p & 0xfe) << 56));
}

static __attribute__((always_inline)) unsigned long *dang_compress_get1(
	unsigned long *value) {
	unsigned long v = (unsigned long) value;
	return (unsigned long *) (v & 0x0000ffffffffffffUL);
}

static __attribute__((always_inline)) unsigned long *dang_compress_get2(
	unsigned long *value) {
	unsigned long v = (unsigned long) value;
	return (unsigned long *) ((v & 0x0000ffffffffff00UL) | ((v >> 48) & 0xfe));
}

static __attribute__((always_inline)) unsigned long *dang_compress_get3(
	unsigned long *value) {
	unsigned long v = (unsigned long) value;
	return (unsigned long *) ((v & 0x0000ffffffffff00UL) | ((v >> 56) & 0xfe));
}

static __attribute__((always_inline)) int dang_compress_has2(
	unsigned long *value) {
	unsigned long v = (unsigned long) value;
	return (v & 0x0001000000000000UL) != 0;
}

static __attribute__((always_inline)) int dang_compress_has3(
	unsigned long *value) {
	unsigned long v = (unsigned long) value;
	return (v & 0x0100000000000000UL) != 0;
}

static __attribute__((always_inline)) int dang_compress_matchpfx(
	unsigned long *value, unsigned long *ptr_addr) {
	unsigned long v = (unsigned long) value;
	unsigned long p = (unsigned long) ptr_addr;
	return ((p ^ v) & 0x0000ffffffffff00UL) == 0;
}

static __attribute__((always_inline)) int dang_compress_match1(
	unsigned long *value, unsigned long *ptr_addr) {
	unsigned long v = (unsigned long) value;
	unsigned long p = (unsigned long) ptr_addr;
	return ((p ^ v) & 0x00000000000000ffUL) == 0;
}

static __attribute__((always_inline)) int dang_compress_match2(
	unsigned long *value, unsigned long *ptr_addr) {
	unsigned long v = (unsigned long) value;
	unsigned long p = (unsigned long) ptr_addr;
	return ((p ^ (v >> 48)) & 0x00000000000000feUL) == 0;
}

static __attribute__((always_inline)) int dang_compress_match3(
	unsigned long *value, unsigned long *ptr_addr) {
	unsigned long v = (unsigned long) value;
	unsigned long p = (unsigned long) ptr_addr;
	return ((p ^ (v >> 56)) & 0x00000000000000feUL) == 0;
}
 
static __attribute__((always_inline)) dang_objlog_t *dang_alloc_threadlog(
    dang_objlog_t *prev_log,
    unsigned long thread_id) {
    dang_objlog_t *obj_loglocal;

    STATS(dang_stats.threadlogs++);

    /* Allocate log buffer and append it */
    DANG_MALLOC(obj_loglocal, dang_objlog_t *, sizeof(dang_objlog_t));
    STATS_ALLOC(threadlog, sizeof(dang_objlog_t));
    DANG_GET_THREADID(obj_loglocal->thread_id);
    obj_loglocal->next_log = NULL;
    obj_loglocal->count = 0;
    obj_loglocal->size = 0;
  
    if (prev_log) {
        STATS(dang_stats.threadlogs_next++);
        obj_loglocal->next_log = (dang_objlog_t *)
	    dang_atomic_cmpxchng((unsigned long *) &prev_log->next_log, 
	    (unsigned long) obj_loglocal);
    }

    return obj_loglocal;
}

static __attribute__((always_inline)) unsigned long dang_hashptr(
    unsigned long *ptr_addr) {
    unsigned long p = (unsigned long) ptr_addr;
    return p ^ (p / DANG_HASHTABLE_INITSIZE);
}

static __attribute__((always_inline)) int dang_add_hashtable(
    unsigned long **hashtable,
    unsigned long hashtablesize,
    unsigned long *ptr_addr) {
    unsigned long *entry, **entry_p;
    unsigned long index;

    /* add pointer to hash table; we already know there is space */
    index = dang_hashptr(ptr_addr) & (hashtablesize - 1);
    for (;;) {
	entry_p = hashtable + index;
	entry = *entry_p;
	if (!entry) {
            STATS(dang_stats.hashtable_new++);
	    *entry_p = ptr_addr;
	    return 1;
	}
	if (entry == ptr_addr) {
            STATS(dang_stats.hashtable_dup++);
	    return 0;
	}
        STATS(dang_stats.hashtable_collision++);
	index = (index + DANG_HASHTABLE_SKIP) & (hashtablesize - 1);
    }
}

static __attribute__((always_inline)) void dang_alloc_hashtable(
    dang_objlog_t *obj_loglocal) {
    size_t allocsize;
    unsigned long **hashtable;
    unsigned long hashtablecount = 0;
    unsigned long i;
    unsigned long **log;
    unsigned long logcount;
    unsigned long *value;

    STATS(dang_stats.hashtable_alloc++);
    allocsize = DANG_HASHTABLE_INITSIZE * sizeof(unsigned long *);
    DANG_MALLOC(hashtable, unsigned long **, allocsize);
    STATS_ALLOC(hashtable, allocsize);
    memset(hashtable, 0, allocsize);

    logcount = obj_loglocal->count;
    log = obj_loglocal->staticlog;
    for (i = 0; i < logcount; i++) {
	value = log[i];
	if (dang_add_hashtable(hashtable, DANG_HASHTABLE_INITSIZE, dang_compress_get1(value))) hashtablecount++;
	if (dang_compress_has2(value) && dang_add_hashtable(hashtable, DANG_HASHTABLE_INITSIZE, dang_compress_get2(value))) hashtablecount++;
	if (dang_compress_has3(value) && dang_add_hashtable(hashtable, DANG_HASHTABLE_INITSIZE, dang_compress_get3(value))) hashtablecount++;
    }

    obj_loglocal->count = hashtablecount;
    obj_loglocal->size = DANG_HASHTABLE_INITSIZE;
    obj_loglocal->hashtable = hashtable;
}

static __attribute__((always_inline)) void dang_grow_hashtable(
    dang_objlog_t *obj_loglocal) {
    size_t allocsize;
    unsigned long **entry;
    unsigned long **hashtablenew;
    unsigned long i;
    unsigned long *ptr;
    unsigned long sizeold, sizenew;

    STATS(dang_stats.hashtable_realloc++);
    sizenew = obj_loglocal->size * DANG_HASHTABLE_GROWFACTOR;
    allocsize = sizenew * sizeof(unsigned long *);
    DANG_MALLOC(hashtablenew, unsigned long **, allocsize);
    STATS_ALLOC(hashtable, allocsize);
    memset(hashtablenew, 0, allocsize);

    entry = obj_loglocal->hashtable;
    sizeold = obj_loglocal->size;
    for (i = 0; i < sizeold; i++) {
	ptr = *(entry++);
	if (ptr) dang_add_hashtable(hashtablenew, sizenew, ptr);
    }

    DANG_FREE(obj_loglocal->hashtable);
    STATS_FREE(hashtable, allocsize / DANG_HASHTABLE_GROWFACTOR);

    obj_loglocal->size = sizenew;
    obj_loglocal->hashtable = hashtablenew;
}

static __attribute__((always_inline)) void dang_register_hashtable(
    unsigned long *ptr_addr,
    dang_objlog_t *obj_loglocal) {
    STATS(dang_stats.hashtable_register++);
    if (dang_add_hashtable(
	obj_loglocal->hashtable,
	obj_loglocal->size,
	ptr_addr)) {
	obj_loglocal->count++;
    }
}

static __attribute__((always_inline)) void dang_add_log_partial(
    unsigned long *ptr_addr,
    dang_objlog_t *obj_loglocal,
    unsigned long index,
    unsigned long **log) {
    unsigned long *value = log[index];

    if (!dang_compress_has2(value)) {
	STATS(dang_stats.staticlog_add_partial2++);
	dang_compress_add2(value, ptr_addr);
    } else {
	STATS(dang_stats.staticlog_add_partial3++);
	dang_compress_add3(value, ptr_addr);
    }
    log[index] = (unsigned long *) value;
}

static __attribute__((always_inline)) int dang_match_log_entry(
    unsigned long *ptr_addr,
    dang_objlog_t *obj_loglocal,
    unsigned long **log,
    unsigned long index,
    unsigned long *partialmatch) {
    unsigned long *value = log[index];
    if (!dang_compress_matchpfx(value, ptr_addr)) {
	STATS(dang_stats.staticlog_match_badprefix++);
	return 0;
    }
    if (dang_compress_match1(value, ptr_addr)) {
	STATS(dang_stats.staticlog_match_partial1++);
	return 1;
    }
    if (!dang_compress_has2(value)) {
	STATS(dang_stats.staticlog_match_badpartial1++);
	*partialmatch = index;
	return 0;
    }
    if (dang_compress_match2(value, ptr_addr)) {
	STATS(dang_stats.staticlog_match_partial2++);
	return 1;
    }
    if (!dang_compress_has3(value)) {
	STATS(dang_stats.staticlog_match_badpartial2++);
	*partialmatch = index;
	return 0;
    }
    if (dang_compress_match3(value, ptr_addr)) {
	STATS(dang_stats.staticlog_match_partial3++);
	return 1;
    }
    STATS(dang_stats.staticlog_match_badpartial3++);
    return 0;
}

static __attribute__((always_inline)) int dang_register_log(
    unsigned long *ptr_addr,
    dang_objlog_t *obj_loglocal,
    unsigned long **log,
    unsigned long logsize) {
    unsigned long count, i, lookback, partialmatch = -1UL;

    /* did we encounter this value recently? */
    if (DANG_STATICLOG_LOOKBACK > 0) {
        /* use a constant loop bound if possible to help the optimizer */
	count = obj_loglocal->count;
	lookback = (count < DANG_STATICLOG_LOOKBACK) ? count : DANG_STATICLOG_LOOKBACK;
	for (i = 1; i <= lookback; i++) {
	    if (dang_match_log_entry(ptr_addr, obj_loglocal, log, count - i, &partialmatch)) {
		STATS(dang_stats.staticlog_lookback[i]++);
		return 1;
	    }
	}
    }

    /* compressed: add at partial match site */
    if (partialmatch != -1UL) {
	STATS(dang_stats.staticlog_lookbackpartial[obj_loglocal->count - partialmatch]++);
	dang_add_log_partial(ptr_addr, obj_loglocal, partialmatch, log);
	return 1;
    }

    if (obj_loglocal->count >= logsize) {
	return 0;
    }

    /* add pointer to static log; we already know there is space */
    STATS(dang_stats.staticlog_add_partial1++);
    log[obj_loglocal->count++] = ptr_addr;
    return 1;
}

static void dang_register(
    unsigned long *ptr_addr,
    dang_objlog_t *obj_log) {
    dang_objlog_t *obj_loglocal;
    
    dang_objlog_t *obj_logprev;

    /* Retrieve thread ID. Initialize thread ID, if not initialized */
    if (unlikely(!threadlocal_id)) {
        /* Give thread ID atomically */
        threadlocal_id = dang_atomic_add(&threadglobal_id, 1);
    }

    /* note: obj_log is guaranteed to be non-NULL */
    obj_loglocal = obj_log;
    while (obj_loglocal->thread_id != threadlocal_id) {
        obj_logprev = obj_loglocal;
        obj_loglocal = obj_loglocal->next_log;
	if (unlikely(!obj_loglocal)) {
	    obj_loglocal = dang_alloc_threadlog(obj_logprev, threadlocal_id);
	    break;
	}
    }

    STATS(dang_stats.operation_register++);
    STATS(dang_stats.bins_count[dang_stats_get_bin(obj_loglocal->count)]--);
    if (obj_loglocal->size == 0) {
	if (dang_register_log(ptr_addr, obj_loglocal, obj_loglocal->staticlog, DANG_STATICLOG_ENTRIES)) goto done;
	dang_alloc_hashtable(obj_loglocal);
    } else if (obj_loglocal->count * DANG_HASHTABLE_MAXLOADFRAC > obj_loglocal->size) {
	dang_grow_hashtable(obj_loglocal);
    }
    dang_register_hashtable(ptr_addr, obj_loglocal);

done:
    STATS(dang_stats.bins_count[dang_stats_get_bin(obj_loglocal->count)]++);
}

/*
 * Inline function which will first check the obj_addr validity. 
 * Globals will have 0 metadata. TODO: stack variables currently are not tracked. 
 * Thus, their metadata will be 0. We are tracking all pointers (stored in globals,
 * heap and stack).
 */
void inlinedang_registerptr(unsigned long ptr_addr, unsigned long obj_addr) {
    dang_objlog_t *obj_log;

    if (obj_addr == 0 || obj_addr & DANG_NULL) return;
    
    /* Check for stack and global objects */
    if ((obj_addr - dang_stack_start) < dang_stack_size) {
	STATS(dang_stats.object_stack++);
        return;
    }
    if ((obj_addr - dang_global_start) < dang_global_size) {
	STATS(dang_stats.object_global++);
        return;
    }

#ifndef TRACK_ALL_PTRS
    if ((ptr_addr - dang_stack_start) < dang_stack_size) {
	STATS(dang_stats.pointer_stack++);
        return;
    }
    if ((ptr_addr - dang_global_start) < dang_global_size) {
	STATS(dang_stats.pointer_global++);
        return;
    }
#endif

    /* prevent segfault in metaget if instrumentation was added incorrectly */
    if (unlikely(obj_addr >= (1UL << 48))) {
	STATS(dang_stats.object_badptr++);
	return;
    }

    obj_log = (dang_objlog_t *) metaget_8(obj_addr);
    if (!obj_log) {
	STATS(dang_stats.object_nolog++);
	return;
    }

    dang_register((unsigned long *) ptr_addr, obj_log);
}

static __attribute__((always_inline)) void dang_nullifyptr(
    unsigned long *ptr,
    unsigned long obj_addr,
    unsigned long size) {
    unsigned long ptr_value;

    if ((unsigned long) ptr >= (unsigned long) __builtin_frame_address(0) &&
    	(unsigned long) ptr <= (unsigned long) tcmalloc_stackptr) {
	return;
    }

    if (sigsetjmp(thread_state.jmp, 1)) return;

    thread_state.canjmp = 1;
    ptr_value = (unsigned long) *ptr;
    if (ptr_value - obj_addr < size) {
	STATS(dang_stats.nullify_done++);
        DSAN_ATOMIC_CMPXCHNG_ONCE(ptr, ptr_value, ptr_value | DANG_NULL);
    } else {
	STATS(dang_stats.nullify_stale++);
    }
    thread_state.canjmp = 0;
}

static __attribute__((always_inline)) void dang_nullifyptrs_hashtable(
    dang_objlog_t *obj_loglocal,
    unsigned long obj_addr,
    unsigned long size) {
    unsigned long **entry;
    unsigned long i;
    unsigned long *ptr;

    if (!dang_ignore_free) {
    entry = obj_loglocal->hashtable;
    for (i = 0; i < obj_loglocal->size; i++) {
	ptr = *(entry++);
	if (ptr) dang_nullifyptr(ptr, obj_addr, size);
    }
    }
    DANG_FREE(obj_loglocal->hashtable);
    STATS_FREE(hashtable, obj_loglocal->size * sizeof(unsigned long *));
}

static __attribute__((always_inline)) void dang_nullifyptrs_log(
    dang_objlog_t *obj_loglocal,
    unsigned long obj_addr,
    unsigned long size,
    unsigned long **log) {
    unsigned long i;
    unsigned long *value;

    if (dang_ignore_free) return;

    for (i = 0; i < obj_loglocal->count; i++) {
	value = log[i];
	dang_nullifyptr(dang_compress_get1(value), obj_addr, size);
	if (dang_compress_has2(value)) dang_nullifyptr(dang_compress_get2(value), obj_addr, size);
	if (dang_compress_has3(value)) dang_nullifyptr(dang_compress_get3(value), obj_addr, size);
    }
}

static __attribute__((always_inline)) void dang_nullifyptrs(
    dang_objlog_t *obj_loglocal,
    unsigned long obj_addr,
    unsigned long size) {
    if (obj_loglocal->size > 0) {
	dang_nullifyptrs_hashtable(obj_loglocal, obj_addr, size);
    } else {
	dang_nullifyptrs_log(obj_loglocal, obj_addr, size, obj_loglocal->staticlog);
    }
}

void dang_freeptr(unsigned long obj_addr, unsigned long size) {
    struct sigaction sa_old;

    if (free_flag) {
        free_flag = false;
        return;
    }

    STATS(dang_stats.operation_free++);

    dang_objlog_t *obj_loglocal = (dang_objlog_t *) metaget_8(obj_addr);
    dang_objlog_t *obj_logprev;
    STATS(dang_stats.bins_count_free[dang_stats_get_bin(obj_loglocal->count)]++);

    /* Set metadata to NULL. PerlBEnch has issue. See gperftools
     * tcmalloc.cc for more information.
     */
    metaset_8(obj_addr, size, 0);

    dang_sighandler_set(&sa_old);
 
    while (obj_loglocal) {
        dang_nullifyptrs(obj_loglocal, obj_addr, size);
	obj_logprev = obj_loglocal;
	obj_loglocal = obj_loglocal->next_log;
	DANG_FREE(obj_logprev);
	STATS_FREE(threadlog, sizeof(dang_objlog_t));
    }

    dang_sighandler_unset(&sa_old);
}

void dang_init_heapobj(unsigned long obj_addr, unsigned long size) {

    /* DSAN uses TCmalloc to allocate internal log buffers.
     * Avoid recursion tracking for internal allocations.
     * TODO: This can be done in TCmalloc which will save one internal
     * function call.
     */
    if (malloc_flag) {
        malloc_flag = false;
        return;
    }

    STATS(dang_stats.operation_malloc++);
    STATS(dang_stats.bins_count[dang_stats_get_bin(0)]++);
    STATS(dang_stats.bins_objsize[dang_stats_get_bin(size)]++);

    if (unlikely(!threadlocal_id)) {
        threadlocal_id = dang_atomic_add(&threadglobal_id, 1);
    }
    
    /* Allocate thread log */
    dang_objlog_t *obj_loglocal;
    DANG_MALLOC(obj_loglocal, dang_objlog_t *, sizeof(dang_objlog_t));
    STATS_ALLOC(threadlog, sizeof(dang_objlog_t));
    DANG_GET_THREADID(obj_loglocal->thread_id);
    obj_loglocal->next_log = NULL;
    obj_loglocal->count = 0;
    obj_loglocal->size = 0;

    /* Set metadata value TODO: TCmalloc hook inits it with zero. Skip that hook */
    metaset_8(obj_addr, size, (meta8) obj_loglocal);
}

void *PointerTrackerUninstrumented_malloc(size_t size) {
    void *ptr;
    malloc_flag = true;
    ptr = malloc(size);
    malloc_flag = false;
    return ptr;
}

void *PointerTrackerUninstrumented_calloc(size_t nmemb, size_t size) {
    void *ptr;
    malloc_flag = true;
    ptr = calloc(nmemb, size);
    malloc_flag = false;
    return ptr;
}

void PointerTrackerUninstrumented_free(void *ptr) {
    free_flag = true;
    free(ptr);
    free_flag = false;
    return;
}
