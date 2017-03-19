#ifndef METAPAGETABLE_CORE_H
#define METAPAGETABLE_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#define METALLOC_PAGESHIFT 12
#define METALLOC_PAGESIZE (1 << METALLOC_PAGESHIFT)

#define METALLOC_FIXEDSHIFT 3
#define METALLOC_FIXEDSIZE (1 << METALLOC_FIXEDSHIFT)

//extern unsigned long pageTable[];
#define pageTable ((unsigned long*)(0x400000000000))
extern int is_fixed_compression();
extern void page_table_init();
extern void* allocate_metadata(unsigned long size, unsigned long alignment);
extern void deallocate_metadata(void *ptr, unsigned long size, unsigned long alignment);
extern void set_metapagetable_entries(void *ptr, unsigned long size, void *metaptr, int alignment);
extern unsigned long get_metapagetable_entry(void *ptr);
extern void allocate_metapagetable_entries(void *ptr, unsigned long size);
extern void deallocate_metapagetable_entries(void *ptr, unsigned long size);

#ifdef __cplusplus
}
#endif

#endif /* !METAPAGETABLE_CORE_H */
