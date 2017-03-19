#ifndef DSAN_STATS_H
#define DSAN_STATS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/times.h>
#include <unistd.h>

#define DANG_STATS_BIN_COUNT 32

static __attribute__((always_inline)) unsigned int dang_stats_get_bin(unsigned long value) {
    unsigned int bin = 0;

    while (value > 0) {
	value >>= 1;
	bin++;
    }

    return (bin < DANG_STATS_BIN_COUNT) ? bin : (DANG_STATS_BIN_COUNT - 1);
}

volatile struct {
#define FIELD_ARRAY(name, n) unsigned long name[n];
#define FIELD_BINS(name)     unsigned long name[DANG_STATS_BIN_COUNT];
#define FIELD_ULONG(name)    unsigned long name;
#include "dsan_stats_fields.h"
#undef FIELD_ARRAY
#undef FIELD_BINS
#undef FIELD_ULONG
} dang_stats;

static void dang_stats_print_array(
    FILE *file,
    const char *name,
    volatile unsigned long *array,
    double scale,
    unsigned long n) {
    unsigned long i;

    for (i = 0; i < n; i++) {
	fprintf(file, "%s[%lu]=%.0f\n", name, i, array[i]*scale);
    }
}

static void dang_stats_print_bins(
    FILE *file,
    const char *name,
    volatile unsigned long *bins,
    double scale) {
    unsigned int i;
    unsigned long min, max;

    for (i = 0; i < DANG_STATS_BIN_COUNT; i++) {
	min = (i > 0) ? (1UL << (i - 1)) : 0;
	max = (1UL << i) - 1;
	fprintf(file, "%s[%lu-%lu]=%.0f\n", name, min, max, bins[i]*scale);
    }
}

static void dang_stats_print_ulong(
    FILE *file,
    const char *name,
    unsigned long value,
    double scale) {
    fprintf(file, "%s=%.0f\n", name, value*scale);
}

static void dang_stats_print_clock(
    FILE *file,
    const char *name,
    double value) {
    fprintf(file, "%s=%.3f\n", name, value);
}

static void dang_stats_print(void) {
    const char *path;
    FILE *file;
    double scale;
    struct tms tms = { };
    double clocks_per_sec = sysconf(_SC_CLK_TCK);

    times(&tms);

    path = getenv("DANGSAN_STATS_PATH");
    if (!path) path = "dangsan_stats.txt";

    file = fopen(path, "a");
    if (!file) {
	perror("cannot open dangsan stats file");
	return;
    }

    dang_stats_print_clock(file, "tms_total", (tms.tms_utime + tms.tms_stime) / clocks_per_sec);
    dang_stats_print_clock(file, "tms_utime", tms.tms_utime / clocks_per_sec);
    dang_stats_print_clock(file, "tms_stime", tms.tms_stime / clocks_per_sec);
    if (tms.tms_utime + tms.tms_stime > 0) {
	scale = clocks_per_sec / (tms.tms_utime + tms.tms_stime);
    } else {
	scale = 0;
    }

#define FIELD_ARRAY(name, n) dang_stats_print_array(file, #name, dang_stats.name, 1, n);
#define FIELD_BINS(name)     dang_stats_print_bins (file, #name, dang_stats.name, 1);
#define FIELD_ULONG(name)    dang_stats_print_ulong(file, #name, dang_stats.name, 1);
#include "dsan_stats_fields.h"
#undef FIELD_ARRAY
#undef FIELD_BINS
#undef FIELD_ULONG

#define FIELD_ARRAY(name, n) dang_stats_print_array(file, #name "/sec", dang_stats.name, scale, n);
#define FIELD_BINS(name)     dang_stats_print_bins (file, #name "/sec", dang_stats.name, scale);
#define FIELD_ULONG(name)    dang_stats_print_ulong(file, #name "/sec", dang_stats.name, scale);
#include "dsan_stats_fields.h"
#undef FIELD_ARRAY
#undef FIELD_BINS
#undef FIELD_ULONG

    fclose(file);
}

#define STATS_ALLOC(name, sz)										\
	do {												\
	dang_stats.mem_##name##_alloc_count++;								\
	dang_stats.mem_##name##_alloc_size += (sz);							\
	long count = dang_stats.mem_##name##_alloc_count - dang_stats.mem_##name##_free_count;	\
	if ((long) dang_stats.mem_##name##_max_count < count) dang_stats.mem_##name##_max_count = count;	\
	long size = dang_stats.mem_##name##_alloc_size - dang_stats.mem_##name##_free_size;	\
	if ((long) dang_stats.mem_##name##_max_size < size) dang_stats.mem_##name##_max_size = size;		\
	} while (0)
#define STATS_FREE(name, sz)				\
	do {						\
	dang_stats.mem_##name##_free_count++;		\
	dang_stats.mem_##name##_free_size += (sz);	\
	} while (0)

__attribute__((section(".fini_array"), used))
void (*dang_exit_stats)(void) = dang_stats_print;

#endif /* !defined(DSAN_STATS_H) */
