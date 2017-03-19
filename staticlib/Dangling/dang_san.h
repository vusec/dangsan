#ifndef DANG_SAN_H
#define DANG_SAN_H

/* initial size of hash table once static/dynamic logs are exhausted */
#ifndef DANG_HASHTABLE_INITSIZE
#define DANG_HASHTABLE_INITSIZE     128
#endif

/* hash table growth factor once load fraction is too high */
#ifndef DANG_HASHTABLE_GROWFACTOR
#define DANG_HASHTABLE_GROWFACTOR   4
#endif

/* allow hash table load to be at most 1/DANG_HASHTABLE_MAXLOADFRAC */
#ifndef DANG_HASHTABLE_MAXLOADFRAC
#define DANG_HASHTABLE_MAXLOADFRAC  2
#endif

/* skip this many entries in hash table on collision */
#ifndef DANG_HASHTABLE_SKIP
#define DANG_HASHTABLE_SKIP         13
#endif

/* look back in static log to avoid duplicates */
#ifndef DANG_STATICLOG_LOOKBACK
#define DANG_STATICLOG_LOOKBACK     4
#endif

/* look back in static log to avoid duplicates */
#ifndef DANG_OBJLOG_ENTRIES
#define DANG_OBJLOG_ENTRIES            16
#endif



#define DANG_OBJLOG_ENTRIES_ALWAYS 4
#define DANG_STATICLOG_ENTRIES (DANG_OBJLOG_ENTRIES - DANG_OBJLOG_ENTRIES_ALWAYS)

typedef struct dang_objlog dang_objlog_t;       /* Log buffer per-thread and per-object */
struct dang_objlog {
	unsigned long thread_id; /* Per-thread ID */
	dang_objlog_t *next_log; /* Next thread log for the object */
	unsigned long count;     /* Number of entries used in log/hash table */
	unsigned long size;      /* Hash table size */
	union {
		unsigned long *staticlog[DANG_STATICLOG_ENTRIES];
		unsigned long **hashtable;
	};
};

#endif /* !DANG_SAN_H */
