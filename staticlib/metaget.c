#include <metadata.h>
#include <metapagetable_core.h>

#define unlikely(x)     __builtin_expect((x),0)

unsigned long metabaseget (unsigned long ptrInt) {
    unsigned long page = ptrInt / METALLOC_PAGESIZE;
    unsigned long entry = pageTable[page];
    return entry;
}

#define CREATE_METAGET(size)                        \
meta##size metaget_##size (unsigned long ptrInt) {  \
    unsigned long page = ptrInt / METALLOC_PAGESIZE;\
    unsigned long entry = pageTable[page];          \
    if (unlikely(entry == 0)) {                     \
        meta##size zero;                            \
        for (int i = 0; i < sizeof(meta##size) /    \
                        sizeof(unsigned long); ++i) \
            ((unsigned long*)&zero)[i] = 0;         \
        return zero;                                \
    }                                           \
    unsigned long alignment = entry & 0xFF;         \
    char *metabase = (char*)(entry >> 8);           \
    unsigned long pageOffset = ptrInt -             \
                        (page * METALLOC_PAGESIZE); \
    char *metaptr = metabase + ((pageOffset >>      \
                                    alignment) *    \
                        size);                      \
    return *(meta##size *)metaptr;                  \
}

CREATE_METAGET(1)
CREATE_METAGET(2)
CREATE_METAGET(4)
CREATE_METAGET(8)
CREATE_METAGET(16)

#define CREATE_METAGET_DEEP(size)                       \
meta##size metaget_deep_##size (unsigned long ptrInt) { \
    unsigned long page = ptrInt / METALLOC_PAGESIZE;    \
    unsigned long entry = pageTable[page];              \
    /*if (unlikely(entry == 0)) {                         \
        meta##size zero;                                \
        for (int i = 0; i < sizeof(meta##size) /        \
                        sizeof(unsigned long); ++i)     \
            ((unsigned long*)&zero)[i] = 0;             \
        return zero;                                    \
    }*/                                                   \
    unsigned long alignment = entry & 0xFF;             \
    char *metabase = (char*)(entry >> 8);               \
    unsigned long pageOffset = ptrInt -                 \
                            (page * METALLOC_PAGESIZE); \
    char *metaptr = metabase + ((pageOffset >>          \
                                    alignment) *        \
                        sizeof(unsigned long));         \
    unsigned long deep = *(unsigned long*)metaptr;      \
    /*if (unlikely(deep == 0)) {                          \
        meta##size zero;                                \
        for (int i = 0; i < sizeof(meta##size) /        \
                        sizeof(unsigned long); ++i)     \
            ((unsigned long*)&zero)[i] = 0;             \
        return zero;                                    \
    }*/                                                   \
    return *(meta##size *)deep;                         \
}

CREATE_METAGET_DEEP(8)
//CREATE_METAGET_DEEP(16)
//CREATE_METAGET_DEEP(32)

#define CREATE_METAGET_FIXED(size)                          \
meta##size metaget_fixed_##size (unsigned long ptrInt) {    \
    unsigned long pos = ptrInt / METALLOC_FIXEDSIZE;        \
    char *metaptr = ((char*)pageTable) + pos;               \
    return *(meta##size *)metaptr;                          \
}

CREATE_METAGET_FIXED(1)
CREATE_METAGET_FIXED(2)
CREATE_METAGET_FIXED(4)
CREATE_METAGET_FIXED(8)

#define CREATE_METAGET_BASE(size)                       \
meta##size metaget_base_##size (unsigned long ptrInt,   \
                        unsigned long entry,            \
                        unsigned long oldPtrInt) {      \
    unsigned long page = oldPtrInt / METALLOC_PAGESIZE; \
    /*if (unlikely(entry == 0)) {                     \
        meta##size zero;                            \
        for (int i = 0; i < sizeof(meta##size) /    \
                        sizeof(unsigned long); ++i) \
            ((unsigned long*)&zero)[i] = 0;         \
        return zero;                                \
    }*/                                               \
    unsigned long alignment = entry & 0xFF;             \
    char *metabase = (char*)(entry >> 8);               \
    unsigned long pageOffset = ptrInt -                 \
                        (page * METALLOC_PAGESIZE);     \
    char *metaptr = metabase + ((pageOffset >>          \
                                    alignment) *        \
                        size);                          \
    return *(meta##size *)metaptr;                      \
}

CREATE_METAGET_BASE(1)
CREATE_METAGET_BASE(2)
CREATE_METAGET_BASE(4)
CREATE_METAGET_BASE(8)
//CREATE_METAGET(16)

#define CREATE_METAGET_BASE_DEEP(size)                  \
meta##size metaget_base_deep_##size (                   \
                        unsigned long ptrInt,           \
                        unsigned long entry,            \
                        unsigned long oldPtrInt) {      \
    unsigned long page = oldPtrInt / METALLOC_PAGESIZE; \
    /*if (unlikely(entry == 0)) {                         \
        meta##size zero;                                \
        for (int i = 0; i < sizeof(meta##size) /        \
                        sizeof(unsigned long); ++i)     \
            ((unsigned long*)&zero)[i] = 0;             \
        return zero;                                    \
    }*/                                                   \
    unsigned long alignment = entry & 0xFF;             \
    char *metabase = (char*)(entry >> 8);               \
    unsigned long pageOffset = ptrInt -                 \
                            (page * METALLOC_PAGESIZE); \
    char *metaptr = metabase + ((pageOffset >>          \
                                    alignment) *        \
                        sizeof(unsigned long));         \
    unsigned long deep = *(unsigned long*)metaptr;      \
    /*if (unlikely(deep == 0)) {                          \
        meta##size zero;                                \
        for (int i = 0; i < sizeof(meta##size) /        \
                        sizeof(unsigned long); ++i)     \
            ((unsigned long*)&zero)[i] = 0;             \
        return zero;                                    \
    }*/                                                   \
    return *(meta##size *)deep;                         \
}

CREATE_METAGET_BASE_DEEP(8)
//CREATE_METAGET_BASE_DEEP(16)
//CREATE_METAGET_BASE_DEEP(32)



