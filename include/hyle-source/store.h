#ifndef HYLE_SOURCE_STORE_H
#define HYLE_SOURCE_STORE_H

#include <stddef.h>
#include <stdint.h>

struct hyle_source_def_s;
typedef struct hyle_source_store_s hyle_source_store_t;

/*
 * Pluggable Storage Driver Operations for Hyle Source Persistence.
 * Allows decoupling persistence (filesystem directories, in-memory tables,
 * databases) from the Hyle dataset query and indexing engine.
 */
typedef struct {
	/* Scan storage backend and populate def->source_hd with all known item IDs.
	 * Returns 0 on success, negative error code on failure. */
	int (*scan)(hyle_source_store_t *store, const struct hyle_source_def_s *def);

	/* Load record for `id` into row map `row_out` (or load item into cache).
	 * Returns 0 on success, negative error code on failure. */
	int (*load)(hyle_source_store_t *store, const struct hyle_source_def_s *def,
	            const char *id, unsigned *row_out);

	/* Persist an entire record `id` represented by `data_handle` (qmap).
	 * Returns 0 on success, negative error code on failure. */
	int (*put)(hyle_source_store_t *store, const struct hyle_source_def_s *def,
	           const char *id, unsigned data_handle);

	/* Update or persist a single field value for record `id`.
	 * Returns 0 on success, negative error code on failure. */
	int (*put_field)(hyle_source_store_t *store,
	                 const struct hyle_source_def_s *def, const char *id,
	                 const char *field, const char *value);

	/* Delete record `id` from persistent storage.
	 * Returns 0 on success, negative error code on failure. */
	int (*del)(hyle_source_store_t *store, const struct hyle_source_def_s *def,
	           const char *id);
} hyle_source_store_ops_t;

/* Storage instance bundling operations and driver-specific state */
struct hyle_source_store_s {
	const hyle_source_store_ops_t *ops;
	void *user;
};

/* Filesystem storage driver (var/<dataset>/<id>/ layout) */
const hyle_source_store_ops_t *hyle_source_store_fs_ops(void);
hyle_source_store_t hyle_source_store_fs(const char *items_path);

/* In-memory storage driver (useful for tests and ephemeral datasets) */
const hyle_source_store_ops_t *hyle_source_store_mem_ops(void);
hyle_source_store_t hyle_source_store_mem(void);

#endif

