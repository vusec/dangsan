#include <stdint.h>
#include <stdlib.h>
#include <metapagetable.h>

typedef uint8_t meta1;
typedef uint16_t meta2;
typedef uint32_t meta4;
typedef uint64_t meta8;

static void set_metadata(void *ptr, void *deepmetadata, unsigned long size, unsigned char value) {
  if (!FLAGS_METALLOC_FIXEDCOMPRESSION) {
      unsigned long page = (unsigned long)ptr / METALLOC_PAGESIZE;
      unsigned long entry = pageTable[page];
      unsigned long alignment = entry & 0xFF;
      char *metabase = (char*)(entry >> 8);
      char *metaptr = metabase + ((((unsigned long)ptr - (page * METALLOC_PAGESIZE)) >> alignment) * FLAGS_METALLOC_METADATABYTES);
      unsigned long metasize = ((size + (1 << (alignment)) - 1) >> alignment);
      if (FLAGS_METALLOC_DEEPMETADATA) {
        unsigned long *metadata_loc = (unsigned long*)deepmetadata;
        for (unsigned long i = 0; i < metasize; ++i)
            ((unsigned long*)metaptr)[i] = (unsigned long)metadata_loc;
	if (metadata_loc != 0) {
		for (unsigned long j = 0; j < FLAGS_METALLOC_DEEPMETADATABYTES / sizeof(unsigned long); ++j)
		  metadata_loc[j] = value;
	}
      } else {
        for (unsigned long i = 0; i < metasize; ++i)
          for (unsigned long j = 0; j < FLAGS_METALLOC_METADATABYTES; ++j)
            metaptr[i * FLAGS_METALLOC_METADATABYTES + j] = value;
      }
  } else {
      unsigned long pos = (unsigned long)ptr / METALLOC_FIXEDSIZE;
      char *metaptr = ((char*)pageTable) + pos;
      unsigned long metasize = ((size + METALLOC_FIXEDSIZE - 1) / METALLOC_FIXEDSIZE);
      for (unsigned long i = 0; i < metasize; ++i)
        for (unsigned long j = 0; j < FLAGS_METALLOC_METADATABYTES; ++j)
          metaptr[i * FLAGS_METALLOC_METADATABYTES + j] = value;
  }
}

void default_alloc_hook(void* ptr, void *deepmetadata, unsigned long content_size, unsigned long allocation_size) {
    set_metadata(ptr, deepmetadata, allocation_size, 0);
}
