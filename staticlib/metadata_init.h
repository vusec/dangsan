#include <stdlib.h>
#include <stdio.h>
#include <metadata.h>
#include <metapagetable_core.h>

static void initialize_metadata(char *start, char *end) {
        if (start > end) {
            fprintf(stderr, "initialize_metadata: bad address range %p-%p\n", start, end);
	    exit(-1);
        }

        unsigned long page_align_offset = METALLOC_PAGESIZE - 1;
        unsigned long page_align_mask = ~((unsigned long)METALLOC_PAGESIZE - 1);
        char *aligned_start = (void*)((unsigned long)start & page_align_mask);
        unsigned long aligned_size = ((end - aligned_start) + page_align_offset) & page_align_mask;
        void *metadata = allocate_metadata(aligned_size, GLOBALALIGN);
        set_metapagetable_entries(aligned_start, aligned_size, metadata, GLOBALALIGN);
}

