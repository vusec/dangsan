#define FIELD_MEM(name)				\
	FIELD_ULONG(mem_##name##_alloc_count);	\
	FIELD_ULONG(mem_##name##_alloc_size);	\
	FIELD_ULONG(mem_##name##_free_count);	\
	FIELD_ULONG(mem_##name##_free_size);	\
	FIELD_ULONG(mem_##name##_max_count);	\
	FIELD_ULONG(mem_##name##_max_size);

FIELD_BINS(bins_count);
FIELD_BINS(bins_count_free);
FIELD_BINS(bins_objsize);
FIELD_ULONG(hashtable_alloc);
FIELD_ULONG(hashtable_collision);
FIELD_ULONG(hashtable_dup);
FIELD_ULONG(hashtable_new);
FIELD_ULONG(hashtable_realloc);
FIELD_ULONG(hashtable_register);
FIELD_MEM(threadlog);
FIELD_MEM(hashtable);
FIELD_ULONG(nullify_done);
FIELD_ULONG(nullify_stale);
FIELD_ULONG(object_badptr);
FIELD_ULONG(object_global);
FIELD_ULONG(object_nolog);
FIELD_ULONG(object_stack);
FIELD_ULONG(operation_free);
FIELD_ULONG(operation_malloc);
FIELD_ULONG(operation_register);
FIELD_ULONG(pointer_global);
FIELD_ULONG(pointer_stack);
FIELD_ULONG(segfaults);
FIELD_ULONG(staticlog_add_partial1);
FIELD_ULONG(staticlog_add_partial2);
FIELD_ULONG(staticlog_add_partial3);

FIELD_ARRAY(staticlog_lookback, DANG_STATICLOG_LOOKBACK + 1);
FIELD_ARRAY(staticlog_lookbackpartial, DANG_STATICLOG_LOOKBACK + 1);
FIELD_ULONG(staticlog_match_badpartial1);
FIELD_ULONG(staticlog_match_badpartial2);
FIELD_ULONG(staticlog_match_badpartial3);
FIELD_ULONG(staticlog_match_badprefix);
FIELD_ULONG(staticlog_match_partial1);
FIELD_ULONG(staticlog_match_partial2);
FIELD_ULONG(staticlog_match_partial3);
FIELD_ULONG(threadlogs);
FIELD_ULONG(threadlogs_next);
