#ifndef DSAN_COMMON_H
#define DSAN_COMMON_H

#include "metapagetable.h"
#include <stdio.h>
#include <stdlib.h>

extern __thread bool malloc_flag;
extern __thread bool free_flag;
#define DANG_MALLOC(ptr, type, size)       \
        malloc_flag = true;                \
        ptr = (type) malloc(size)
#define DANG_FREE(ptr)                     \
        free_flag = true, free(ptr)

#define DANG_NULL               (unsigned long)0x8000000000000000

extern void dang_freeptr(unsigned long obj_adr, unsigned long size);
extern void dang_init_heapobj(unsigned long obj_addr, unsigned long size);

/* 
 * Initialize function to allocatee mempool, hashtable or any other memory requirement.
 */
void dang_initialize(void) {
        
        /* Set malloc post hook */
        metalloc_malloc_posthook = dang_init_heapobj;

        /* Set free pre hook */
        metalloc_free_prehook = dang_freeptr;
}

#endif /* !DSAN_COMMON_H */
