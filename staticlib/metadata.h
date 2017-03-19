
#ifndef METADATA_H
#define METADATA_H
     
#include <stdint.h>
#include <string.h>

typedef uint8_t meta1;
typedef uint16_t meta2;
typedef uint32_t meta4;
typedef uint64_t meta8;
typedef struct{
    uint64_t a;
    uint64_t b;
} meta16;
typedef struct{
    uint64_t a;
    uint64_t b;
    uint64_t c;
    uint64_t d;
} meta32;

#define STACKALIGN ((unsigned long)6)
#define STACKALIGN_LARGE ((unsigned long)12)
#define GLOBALALIGN ((unsigned long)3)

#define META_FUNCTION_NAME_INTERNAL(function, size) #function"_"#size
#define META_FUNCTION_NAME(function, size) META_FUNCTION_NAME_INTERNAL(function, size)

static char const *METADATAFUNCS[] = {  "metaset_1", "metaset_2", "metaset_4", "metaset_8", "metaset_16", 
                                        "metaset_alignment_1", "metaset_alignment_2", "metaset_alignment_4", "metaset_alignment_8", "metaset_alignment_16",
                                        "metaset_alignment_safe_1", "metaset_alignment_safe_2", "metaset_alignment_safe_4", "metaset_alignment_safe_8", "metaset_alignment_safe_16",
                                        "metaset_fast_1", "metaset_fast_2", "metaset_fast_4", "metaset_fast_8", "metaset_fast_16",
                                        "metaset_fixed_1", "metaset_fixed_2", "metaset_fixed_4", "metaset_fixed_8", "metaset_fixed_16",
                                        "metabaseget", 
                                        "metaget_1", "metaget_2", "metaget_4", "metaget_8", "metaget_16",
                                        "metaget_deep_8", "metaget_deep_16", "metaget_deep_32",
                                        "metaget_fixed_1", "metaget_fixed_2", "metaget_fixed_4", "metaget_fixed_8",
                                        "metaget_base_1", "metaget_base_2", "metaget_base_4", "metaget_base_8", "metaget_base_16",
                                        "metaget_base_deep_8", "metaget_base_deep_16", "metaget_base_deep_32",
                                        "initialize_global_metadata", "initialize_metadata", "unsafe_stack_alloc_meta", "unsafe_stack_free_meta",
					"inlinedang_init_globalobj", "inlinedang_registerptr"};
__attribute__ ((unused)) static int ISMETADATAFUNC(const char *name) {

	/* All dangling nullification functions starts with dang_ .
	 * This is a best effort guess. Application can start with dang_
	 */
	if (!strncmp(name, "dang_", 5))
		return 1;

    for (unsigned int i = 0; i < (sizeof(METADATAFUNCS) / sizeof(METADATAFUNCS[0])); ++i) {
        int different = 0;
        char const *lhs = METADATAFUNCS[i];
        const char *rhs = name;
        while (*lhs != 0 && *rhs != 0)
            if (*lhs++ != *rhs++) {
                different = 1;
                break; 
            }
        if (*lhs != *rhs)
            different = 1;
        if (different == 0)
            return 1;
    }
    return 0;
}

/* Need declaration of get/set functions */
#define DECLARE_METAGET(size) 				\
meta##size metaget_##size (unsigned long ptrInt); 	

DECLARE_METAGET(1)
DECLARE_METAGET(2)
DECLARE_METAGET(4)
DECLARE_METAGET(8)
DECLARE_METAGET(16)
DECLARE_METAGET(32)

#define DECLARE_METASET(size)                        \
unsigned long metaset_##size (unsigned long ptrInt, \
        unsigned long count, meta##size value);     

DECLARE_METASET(1)
DECLARE_METASET(2)
DECLARE_METASET(4)
DECLARE_METASET(8)
DECLARE_METASET(16)
DECLARE_METASET(32)

unsigned long metabaseget (unsigned long ptrInt);

#endif /* !METADATA_H */
