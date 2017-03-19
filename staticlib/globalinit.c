#define _GNU_SOURCE
#define __USE_GNU

#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <stdio.h>
#include <metadata.h>
#include <metapagetable_core.h>

#include "metadata_init.h"

/* DangSan: */
extern void dang_initialize();
unsigned long dang_global_start;
unsigned long dang_global_size;

__attribute__ ((visibility("hidden"))) extern char _end;

__attribute__((visibility ("hidden"), constructor(-1))) void initialize_global_metadata() {
    static int initialized;

    /* use both constructor and preinit_array to be first in executables and still work in shared objects */
    if (initialized) return;
    initialized = 1;

    if (!is_fixed_compression()) {
	/* code, data, bss, ... all assumed to be together */
        Dl_info info = {};
        if (!dladdr(initialize_global_metadata, &info)) {
            perror("initialize_global_metadata: dladdr failed");
	    exit(-1);
        }
	char *global_start = info.dli_fbase;
	char *global_end = &_end;
	initialize_metadata(global_start, global_end);

        /* DangSan: 
         * Global start and end is required to find global objects/ptrs.
         */
        dang_global_start = (unsigned long) global_start;
        dang_global_size = (unsigned long) (global_end - global_start);

        /* stack metadata should not be used as objects are supposed to be moved
         * to an alternative stack, but we need to be prepared for callbacks
         * from JIT code and uninstrumened libraries
         */
        struct rlimit rlim;
        if (getrlimit(RLIMIT_STACK, &rlim) != 0 || rlim.rlim_cur <= 0 ||
            rlim.rlim_cur >= 0x80000000) {
            rlim.rlim_cur = 0x100000; /* 1MB if no limit set */
        }
        char *stack_end;
        __asm__("mov %%rsp, %0" : "=R" (stack_end));
        char *stack_start = stack_end - rlim.rlim_cur;
        initialize_metadata(stack_start, stack_end);
    }

     /* Comment is valid for Dangling pointer design 1.
      * FIXME: All metadata for global variables are allocated and initialized just after this function
      * call(GlobalTracker.cpp in llvm plugins).
      * And, we need to call dang_init_object() for each such allocation.
      * However, dang_init_object() requires spinlock and it should be initialized in .preinit_array
      * But, order of execution of .preinit_array cannot be specified (TODO: find it)
      */
    dang_initialize();
    return;
}

__attribute__((section(".preinit_array"),
               used)) void (*initialize_global_metadat_preinit)(void) = initialize_global_metadata;
