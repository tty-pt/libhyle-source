#ifndef HYLE_SOURCE_STORE_H
#define HYLE_SOURCE_STORE_H

/**
 * @file store.h
 * @brief Pluggable storage driver interface for hyle-source.
 *
 * Decouples persistence (filesystem, in-memory, or a database) from the
 * hyle dataset query and indexing engine.
 */

#include <stddef.h>
#include <stdint.h>

struct hyle_source_def_s;
typedef struct hyle_source_store_s hyle_source_store_t;

/**
 * @brief Pluggable storage driver operations for hyle source persistence.
 */
typedef struct {
	/**
	 * @brief Scan the backend and populate def->source_hd with all known ids.
	 * @param[in] store Target store.
	 * @param[in] def   Dataset definition.
	 * @return 0 on success, negative error code on failure.
	 */
	int (*scan)(hyle_source_store_t *store, const struct hyle_source_def_s *def);

	/**
	 * @brief Load record `id` into row map `row_out`.
	 * @param[in]  store   Target store.
	 * @param[in]  def     Dataset definition.
	 * @param[in]  id      Item id.
	 * @param[out] row_out Receives the loaded row handle.
	 * @return 0 on success, negative error code on failure.
	 */
	int (*load)(hyle_source_store_t *store, const struct hyle_source_def_s *def,
	            const char *id, unsigned *row_out);

	/**
	 * @brief Persist an entire record `id` represented by `data_handle`.
	 * @param[in] store       Target store.
	 * @param[in] def         Dataset definition.
	 * @param[in] id          Item id.
	 * @param[in] data_handle Corm handle of the record data.
	 * @return 0 on success, negative error code on failure.
	 */
	int (*put)(hyle_source_store_t *store, const struct hyle_source_def_s *def,
	           const char *id, unsigned data_handle);

	/**
	 * @brief Update or persist a single field value for record `id`.
	 * @param[in] store Target store.
	 * @param[in] def   Dataset definition.
	 * @param[in] id    Item id.
	 * @param[in] field Field name.
	 * @param[in] value New field value.
	 * @return 0 on success, negative error code on failure.
	 */
	int (*put_field)(hyle_source_store_t *store,
	                 const struct hyle_source_def_s *def, const char *id,
	                 const char *field, const char *value);

	/**
	 * @brief Delete record `id` from persistent storage.
	 * @param[in] store Target store.
	 * @param[in] def   Dataset definition.
	 * @param[in] id    Item id.
	 * @return 0 on success, negative error code on failure.
	 */
	int (*del)(hyle_source_store_t *store, const struct hyle_source_def_s *def,
	           const char *id);
} hyle_source_store_ops_t;

/**
 * @brief Storage instance bundling operations and driver-specific state.
 */
struct hyle_source_store_s {
	/** Driver operations. */
	const hyle_source_store_ops_t *ops;
	/** Driver-specific state. */
	void *user;
};

/**
 * @brief Filesystem storage driver ops (var/<dataset>/<id>/ layout).
 * @return The filesystem driver ops table.
 */
const hyle_source_store_ops_t *hyle_source_store_fs_ops(void);

/**
 * @brief Construct a filesystem store.
 * @param[in] items_path Base items directory.
 * @return A filesystem-backed store instance.
 */
hyle_source_store_t hyle_source_store_fs(const char *items_path);

/**
 * @brief In-memory storage driver ops (tests and ephemeral datasets).
 * @return The in-memory driver ops table.
 */
const hyle_source_store_ops_t *hyle_source_store_mem_ops(void);

/**
 * @brief Construct an in-memory store.
 * @return An in-memory store instance.
 */
hyle_source_store_t hyle_source_store_mem(void);

#endif

