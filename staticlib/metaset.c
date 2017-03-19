#include <metadata.h>
#include <metapagetable_core.h>

#define CREATE_METASET(size)                        \
unsigned long metaset_##size (unsigned long ptrInt, \
        unsigned long count, meta##size value) {    \
    unsigned long page = ptrInt / METALLOC_PAGESIZE;\
    unsigned long entry = pageTable[page];          \
    unsigned long alignment = entry & 0xFF;         \
    char *metabase = (char*)(entry >> 8);           \
    unsigned long pageOffset = ptrInt -             \
                        (page * METALLOC_PAGESIZE); \
    char *metaptr = metabase + ((pageOffset >>      \
                                    alignment) *    \
                        size);                      \
    unsigned long metasize = ((count +              \
                    (1 << (alignment)) - 1) >>      \
                alignment);                         \
    for (unsigned long i = 0; i < metasize; ++i) {  \
        *(meta##size *)metaptr  = value;   \
        metaptr += size;                            \
    }                                               \
    return entry;                                   \
}

CREATE_METASET(1)
CREATE_METASET(2)
CREATE_METASET(4)
CREATE_METASET(8)
CREATE_METASET(16)

#define CREATE_METASET_ALIGNMENT(size, suffix)      \
unsigned long metaset_alignment_##suffix##size (    \
        unsigned long ptrInt,\
        unsigned long count, meta##size value,      \
        unsigned long alignment) {                  \
    unsigned long page = ptrInt / METALLOC_PAGESIZE;\
    unsigned long entry = pageTable[page];          \
    METASET_CHECK                                   \
    char *metabase = (char*)(entry >> 8);           \
    unsigned long pageOffset = ptrInt -             \
                        (page * METALLOC_PAGESIZE); \
    char *metaptr = metabase + ((pageOffset >>      \
                                    alignment) *    \
                        size);                      \
    unsigned long metasize = ((count +              \
                    (1 << (alignment)) - 1) >>      \
                alignment);                         \
    for (unsigned long i = 0; i < metasize; ++i) {  \
        *(meta##size *)metaptr  = value;   \
        metaptr += size;                            \
    }                                               \
    return entry;                                   \
}

#define METASET_CHECK
CREATE_METASET_ALIGNMENT(1, )
CREATE_METASET_ALIGNMENT(2, )
CREATE_METASET_ALIGNMENT(4, )
CREATE_METASET_ALIGNMENT(8, )
CREATE_METASET_ALIGNMENT(16, )
#undef METASET_CHECK
#define METASET_CHECK if (!entry) return 0;
CREATE_METASET_ALIGNMENT(1, safe_)
CREATE_METASET_ALIGNMENT(2, safe_)
CREATE_METASET_ALIGNMENT(4, safe_)
CREATE_METASET_ALIGNMENT(8, safe_)
CREATE_METASET_ALIGNMENT(16, safe_)
#undef METASET_CHECK

#define CREATE_METASET_FAST(size)              \
unsigned long metaset_fast_##size (            \
        unsigned long ptrInt,\
        unsigned long count, meta##size value,      \
        unsigned long alignment,                    \
        unsigned long entry,                        \
        unsigned long oldPtrInt) {                  \
    unsigned long page = oldPtrInt / METALLOC_PAGESIZE; \
    char *metabase = (char*)(entry >> 8);           \
    long pageOffset = ptrInt -                 \
                            (page * METALLOC_PAGESIZE); \
    char *metaptr = metabase + ((pageOffset >>      \
                                    alignment) *    \
                        size);                      \
    unsigned long metasize = ((count +              \
                    (1 << (alignment)) - 1) >>      \
                alignment);                         \
    for (unsigned long i = 0; i < metasize; ++i) {  \
        *(meta##size *)metaptr  = value;   \
        metaptr += size;                            \
    }                                               \
    return entry;                                   \
}

CREATE_METASET_FAST(1)
CREATE_METASET_FAST(2)
CREATE_METASET_FAST(4)
CREATE_METASET_FAST(8)
CREATE_METASET_FAST(16)

#define CREATE_METASET_FIXED(size)                  \
unsigned long metaset_fixed_##size (                \
        unsigned long ptrInt,                       \
        unsigned long count, meta##size value) {    \
    unsigned long pos = ptrInt / METALLOC_FIXEDSIZE;\
    char *metaptr = ((char*)pageTable) + pos * size;\
    unsigned long metasize = ((count +              \
                    METALLOC_FIXEDSIZE - 1) /       \
                        METALLOC_FIXEDSIZE);        \
    for (unsigned long i = 0; i < metasize; ++i) {  \
        *(meta##size *)metaptr  = value;   \
        metaptr += size;                            \
    }                                               \
    return 0;                                       \
}

CREATE_METASET_FIXED(1)
CREATE_METASET_FIXED(2)
CREATE_METASET_FIXED(4)
CREATE_METASET_FIXED(8)
CREATE_METASET_FIXED(16)
