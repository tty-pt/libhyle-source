#ifndef LIBHYLE_SOURCE_H
#define LIBHYLE_SOURCE_H

/**
 * @file hyle_source.h
 * @brief Persistence layer for hyle datasets.
 *
 * Owns storage drivers (filesystem, in-memory), dataset definitions,
 * access control, and the CRUD entry points on top of hyle + corm.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <json-c/json.h>
#include <hyle/field.h>
#include <hyle/schema.h>
#include <hyle-source/picker.h>
#include "store.h"

/**
 * @brief Access policy of a hyle source dataset.
 */
typedef enum {
	/** Dataset readable/writable by anonymous visitors. */
	HYLE_SOURCE_ACCESS_PUBLIC = 0,
	/** Dataset requires a login. */
	HYLE_SOURCE_ACCESS_LOGIN,
} hyle_source_access_policy_t;

/**
 * @brief Outcome of an access-control decision.
 */
typedef enum {
	/** Access granted. */
	HYLE_SOURCE_ACCESS_RESULT_ALLOW = 0,
	/** Request rejected for missing authentication. */
	HYLE_SOURCE_ACCESS_RESULT_UNAUTHORIZED,
	/** Request rejected for insufficient rights. */
	HYLE_SOURCE_ACCESS_RESULT_FORBIDDEN,
} hyle_source_access_result_t;

/** @brief Inverse-kind compatibility alias. */
#define HYLE_SOURCE_FIELD_KIND_INVERSE HYLE_KIND_INVERSE

/**
 * @brief Authorization-relevant field metadata of a dataset.
 */
typedef struct {
	/** Field name. */
	const char *name;
	/** Attached file name, or NULL. */
	const char *file;
	/** Field category. */
	hyle_field_type_t type;
	/** Non-zero when the field may be written. */
	int writable;
	/** Source id targeted by references. */
	const char *target_source;
	/** Inverse field name. */
	const char *inverse_name;
	/** Non-zero when a value is mandatory. */
	int required;
	/** Inclusive lower int bound. */
	int64_t min;
	/** Inclusive upper int bound. */
	int64_t max;
	/** Minimum string length. */
	size_t min_length;
	/** Maximum string length. */
	size_t max_length;
	/** POSIX regex constraint. */
	const char *pattern;
	/** Display/filter style hint. */
	const char *filter_style;
	/** Filter matching mode. */
	const char *filter_mode;
	/** Derive-provider key. */
	const char *derive_key;
} hyle_source_field_t;

/**
 * @brief One named column of a list view.
 */
typedef struct {
	/** Field key. */
	const char *name;
	/** Column header label. */
	const char *label;
} hyle_source_list_field_t;

/**
 * @brief List-view configuration of a dataset.
 */
typedef struct {
	/** Row display name. */
	const char *display_name;
	/** Column descriptors. */
	const hyle_source_list_field_t *fields;
	/** Column count. */
	size_t field_count;
	/** Default sort key. */
	const char *default_sort;
	/** Content field for rich displays. */
	const char *content_field;
	/** Content label. */
	const char *content_label;
	/** Content placeholder text. */
	const char *content_placeholder;
} hyle_source_list_view_t;

/* Framework-neutral unified descriptor alias */
/** @brief Framework-neutral unified descriptor alias. */
typedef hyle_schema_desc_t hyle_source_desc_t;

/**
 * @brief Definition of one hyle source dataset.
 */
typedef struct hyle_source_def_s {
	/** Dataset id. */
	const char *id;
	/** Primary key field name. */
	const char *key_field;
	/** Persistence base path. */
	const char *items_path;
	/** Access policy. */
	hyle_source_access_policy_t access_policy;
	/** Authorization-relevant field metadata. */
	const hyle_source_field_t *fields;
	/** Field metadata count. */
	size_t field_count;
	/** Corm handle of item records. */
	unsigned source_hd;
	/** Corm handle of per-item fields. */
	unsigned fields_hd;
	/** Corm handle of schema metadata. */
	unsigned schema_hd;
	/** Record-aware corm type id. */
	uint32_t record_id;
	/** Dataset flags. */
	unsigned flags;
	/** List-view configuration. */
	const hyle_source_list_view_t *list_view;
	/** Framework-neutral field descriptors. */
	const hyle_source_desc_t *defs;
	/** Descriptor count. */
	int def_count;
	/** Byte size of one record struct. */
	size_t record_size;
	/** Display field name (list views). */
	char display_field[64];
	/** Caller data. */
	void *user;
	/** Storage driver instance. */
	hyle_source_store_t store;
} hyle_source_def_t;

/**
 * @brief Callback invoked once per registered dataset.
 * @param[in] def  Dataset definition.
 * @param[in] user Caller data.
 * @return Non-zero to stop iteration.
 */
typedef int (*hyle_source_each_cb_t)(const hyle_source_def_t *, void *);

/** @brief Dataset kept in memory only (not persisted). */
#define HYLE_SOURCE_FLAG_VOLATILE 64u
/** @brief Dataset allows client record creation. */
#define HYLE_SOURCE_FLAG_CREATABLE 128u
/** @brief Validation error return sentinel. */
#define HYLE_SOURCE_ERR_VALIDATION -2

/* ── State JSON builder ─────────────────────────────────────────── */

/**
 * @brief Serialization mode of one state field.
 */
typedef enum {
	/** Include the field in state JSON. */
	HYLE_SF_RECORD,
	/** Exclude the field from state JSON. */
	HYLE_SF_EXCLUDE,
	/** Resolve the field to its display name. */
	HYLE_SF_REF_DISPLAY,
} hyle_source_state_kind_t;

/**
 * @brief One entry of a state JSON field spec list.
 */
typedef struct {
	/** Field name. */
	const char *name;
	/** Serialization mode. */
	hyle_source_state_kind_t kind;
} hyle_source_state_field_t;

/**
 * @brief One key/value overlay entry for state JSON.
 */
typedef struct {
	/** JSON key. */
	const char *key;
	/** Non-zero for integer values. */
	int is_int;
	/** Value when is_int. */
	int int_val;
	/** Value when not is_int. */
	const char *str_val;
} hyle_source_state_kv_t;

/**
 * @brief One string-copy mapping used by hyle_json_extract_strings.
 */
typedef struct {
	/** JSON key to read. */
	const char *key;
	/** Destination buffer. */
	char *dest;
	/** Destination capacity. */
	size_t dest_size;
} hyle_json_str_map_t;

/**
 * @brief Copy a set of string fields out of a json-c object.
 *
 * Walks @p map (NULL-key terminated) and copies each present field
 * into its mapped destination via snprintf.
 *
 * @param[in] jo  json-c object to read (may be NULL).
 * @param[in] map Mapping table, NULL-key terminated.
 */
static inline void
hyle_json_extract_strings(json_object *jo, const hyle_json_str_map_t *map)
{
	if (!jo || !map)
		return;
	json_object *jval;
	for (const hyle_json_str_map_t *m = map; m->key; m++) {
		if (json_object_object_get_ex(jo, m->key, &jval))
			snprintf(
			        m->dest, m->dest_size, "%s",
			        json_object_get_string(jval));
	}
}

/* ── Core Engine Functions ───────────────────────────────────────── */

/**
 * @brief Remove all inverse references pointing at an item.
 * @param[in] fd        Corm file descriptor.
 * @param[in] dataset_id Source dataset id.
 * @param[in] item_id   Target item id.
 * @return 0 on success, negative on failure.
 */
int hyle_source_clear_inverse_refs(
    int fd,
    const char *dataset_id,
    const char *item_id);

/**
 * @brief Convert schema descriptors into a corm-openable record layout.
 * @param[in]  defs  Field descriptors.
 * @param[in]  count Descriptor count.
 * @param[out] out   Receives the corm record type configuration.
 * @return 0 on success, negative on failure.
 */
int hyle_source_def_to_corm(
    const hyle_source_desc_t *defs, int count, void *out);

/**
 * @brief Convert schema descriptors into source-field metadata.
 * @param[in]  defs  Field descriptors.
 * @param[in]  count Descriptor count.
 * @param[out] out   Receives the source-field table.
 * @return 0 on success, negative on failure.
 */
int hyle_source_def_to_source_fields(
    const hyle_source_desc_t *defs, int count, void *out);

/**
 * @brief Derive meta-field descriptors for a record instance.
 * @param[in]  defs   Field descriptors.
 * @param[in]  count  Descriptor count.
 * @param[in]  record Source record.
 * @param[out] out    Receives the meta-field table.
 * @return 0 on success, negative on failure.
 */
int hyle_source_def_to_meta_fields(
    const hyle_source_desc_t *defs, int count,
    const void *record, void *out);

/**
 * @brief Build the state-JSON field spec list for a dataset.
 * @param[in]  fields   Field descriptors.
 * @param[out] specs    Receives the state field specs.
 * @param[in]  max_specs Capacity of specs.
 * @return Number of specs written, or negative on overflow.
 */
int hyle_source_build_state_specs(
    const hyle_source_desc_t *fields,
    hyle_source_state_field_t *specs,
    int max_specs);

/**
 * @brief Look up a registered dataset by id.
 * @param[in] dataset_id Dataset id.
 * @return Dataset definition, or NULL.
 */
hyle_source_def_t *hyle_source_find(const char *dataset_id);

/**
 * @brief Whether a dataset allows client record creation.
 * @param[in] dataset_id Dataset id.
 * @return Non-zero when creatable.
 */
int hyle_source_is_creatable(const char *dataset_id);

/**
 * @brief Whether an item id exists in a dataset.
 * @param[in] dataset_id Dataset id.
 * @param[in] item_id    Item id.
 * @return Non-zero when present.
 */
int hyle_source_item_exists(
    const char *dataset_id,
    const char *item_id);

/**
 * @brief Register a dataset definition.
 * @param[in] def Dataset definition.
 * @return Handle on success, negative on failure.
 */
int hyle_source_register_def(const hyle_source_def_t *def);

/**
 * @brief Reload a single row from persistent storage.
 * @param[in] fd         Corm file descriptor.
 * @param[in] dataset_id Dataset id.
 * @param[in] id         Item id.
 * @return 0 on success, negative on failure.
 */
int hyle_source_refresh_row(
    int fd, const char *dataset_id, const char *id);

/**
 * @brief Validate a record against its field constraints.
 * @param[in]  def           Dataset definition.
 * @param[in]  data_handle   Record handle to validate.
 * @param[out] json_errors_out Receives a JSON array of errors (malloc'd).
 * @return 0 when valid, negative when invalid.
 */
int hyle_source_validate_row(
    const hyle_source_def_t *def, unsigned data_handle, char **json_errors_out);

/**
 * @brief Persist an item record (write-through to the store).
 * @param[in] fd           Corm file descriptor.
 * @param[in] dataset_id   Dataset id.
 * @param[in] id           Item id.
 * @param[in] data_handle  Record handle.
 * @return 0 on success, negative on failure.
 */
int hyle_source_update_item(
    int fd, const char *dataset_id,
    const char *id, unsigned data_handle);

/**
 * @brief Delete an item and its references.
 * @param[in] fd      Corm file descriptor.
 * @param[in] def     Dataset definition.
 * @param[in] item_id Item id.
 * @return 0 on success, negative on failure.
 */
int hyle_source_delete_item(
    int fd, const hyle_source_def_t *def, const char *item_id);

/**
 * @brief Register a field as an inverse relation target.
 * @param[in] dataset_id Dataset id.
 * @param[in] field_name Inverse field name.
 * @return 0 on success, negative on failure.
 */
int hyle_ref_field_register(
    const char *dataset_id, const char *field_name);

/**
 * @brief Iterate over all registered datasets.
 * @param[in] cb   Per-dataset callback.
 * @param[in] user Caller data.
 * @return 0 on success, negative on failure.
 */
int hyle_source_for_each(hyle_source_each_cb_t cb, void *user);

/**
 * @brief Run a search query against a dataset.
 * @param[in] dataset_id Dataset id.
 * @param[in] query_str  Search query.
 * @return Corm handle of the result rows, or 0 on failure.
 */
unsigned hyle_source_query_dataset(
    const char *dataset_id,
    const char *query_str);

/**
 * @brief Item-records corm handle of a dataset.
 * @param[in] dataset_id Dataset id.
 * @return Corm handle, or 0 when unregistered.
 */
unsigned hyle_source_get_data_hd(const char *dataset_id);

/**
 * @brief Per-item fields corm handle of a dataset.
 * @param[in] dataset_id Dataset id.
 * @return Corm handle, or 0 when unregistered.
 */
unsigned hyle_source_get_fields_hd(const char *dataset_id);

/**
 * @brief Schema metadata corm handle of a dataset.
 * @param[in] dataset_id Dataset id.
 * @return Corm handle, or 0 when unregistered.
 */
unsigned hyle_source_get_schema_hd(const char *dataset_id);

/**
 * @brief List-view configuration of a dataset.
 * @param[in] dataset_id Dataset id.
 * @return List view, or NULL when unregistered.
 */
const hyle_source_list_view_t *hyle_source_get_list_view(
    const char *dataset_id);

/**
 * @brief Build the JSON object of a single item.
 * @param[in]  def     Dataset definition.
 * @param[in]  item_id Item id.
 * @param[out] out     Receives the item JSON object.
 * @return 0 on success, negative on failure.
 */
int hyle_source_build_item_json(
    const hyle_source_def_t *def,
    const char *item_id,
    json_object **out);

/**
 * @brief Build the state JSON of a single item.
 * @param[in]  dataset_id Dataset id.
 * @param[in]  item_id    Item id.
 * @param[in]  specs      State field specs, or NULL for defaults.
 * @param[out] out        Receives the state JSON object.
 * @return 0 on success, negative on failure.
 */
int hyle_source_build_state_json(
    const char *dataset_id,
    const char *item_id,
    const hyle_source_state_field_t *specs,
    json_object **out);

/**
 * @brief Apply key/value overlays onto a JSON object.
 * @param[in] jo  Target JSON object.
 * @param[in] kvs Overlay table, NULL-key terminated.
 * @return 0 on success, negative on failure.
 */
int hyle_source_state_overlay(
    json_object *jo,
    const hyle_source_state_kv_t *kvs);

/**
 * @brief Overlay descriptors from a state struct onto a JSON object.
 * @param[in] jo       Target JSON object.
 * @param[in] state    Source state struct.
 * @param[in] fields   Field descriptors.
 * @param[in] int_kind Kind of integer state fields.
 * @param[in] str_kind Kind of string state fields.
 * @return 0 on success, negative on failure.
 */
int hyle_source_overlay_from_desc(
    json_object *jo,
    const void *state,
    const hyle_source_desc_t *fields,
    int int_kind,
    int str_kind);

/**
 * @brief Build a JSON array of objects from a record array.
 * @param[in] items     Record array.
 * @param[in] count     Record count.
 * @param[in] elem_size Byte stride of one record.
 * @param[in] fields    Field descriptors.
 * @param[in] int_kind  Kind of integer state fields.
 * @param[in] str_kind  Kind of string state fields.
 * @return json-c array object, or NULL on failure.
 */
json_object *hyle_source_overlay_array(
    const void *items, int count, size_t elem_size,
    const hyle_source_desc_t *fields,
    int int_kind, int str_kind);

/**
 * @brief Resolve a reference field to its display string.
 * @param[in]  dataset_id Dataset id.
 * @param[in]  item_id    Item id.
 * @param[in]  field_name Field name.
 * @param[out] out        Destination buffer.
 * @param[in]  out_sz     Capacity of out.
 * @return 0 on success, negative on failure.
 */
int hyle_source_resolve_ref_display_str(
    const char *dataset_id,
    const char *item_id,
    const char *field_name,
    char *out, size_t out_sz);

/**
 * @brief Resolve reference and inverse fields into display state.
 * @param[in]  dataset_id Dataset id.
 * @param[in]  item_id    Item id.
 * @param[in]  fields     Field descriptors.
 * @param[in]  count      Descriptor count.
 * @param[out] state      State struct to fill.
 * @return 0 on success, negative on failure.
 */
int hyle_source_resolve_meta_display(
    const char *dataset_id,
    const char *item_id,
    const hyle_source_desc_t *fields,
    int count,
    void *state);

/**
 * @brief Read a meta file into a record struct.
 * @param[in]  path        Meta file path.
 * @param[in]  fields      Field descriptors.
 * @param[in]  count       Descriptor count.
 * @param[out] record      Destination record struct.
 * @param[in]  record_size Byte size of record.
 * @return 0 on success, negative on failure.
 */
int hyle_source_meta_read(
    const char *path,
    const hyle_source_desc_t *fields,
    int count,
    void *record,
    size_t record_size);

/**
 * @brief Write a record struct as a meta file.
 * @param[in] path        Meta file path.
 * @param[in] fields      Field descriptors.
 * @param[in] count       Descriptor count.
 * @param[in] record      Source record struct.
 * @return 0 on success, negative on failure.
 */
int hyle_source_meta_write(
    const char *path,
    const hyle_source_desc_t *fields,
    int count,
    const void *record);

/**
 * @brief Set up and register a dataset definition.
 * @param[in] source_id   Dataset id.
 * @param[in] key_field   Primary key field name.
 * @param[in] record_size Byte size of one record struct.
 * @param[in] items_path  Persistence base path.
 * @param[in] defs        Framework-neutral field descriptors.
 * @param[in] field_count Descriptor count.
 * @param[in] flags       Dataset flags.
 * @param[in] list_view   List-view configuration, or NULL.
 * @return Corm record type id on success, 0 on failure.
 */
uint32_t hyle_source_setup(
    const char *source_id,
    const char *key_field,
    size_t record_size,
    const char *items_path,
    const hyle_source_desc_t *defs,
    int field_count,
    unsigned flags,
    const hyle_source_list_view_t *list_view);

/**
 * @brief Collect inverse keys referencing a target position.
 * @param[in]  dataset_id  Dataset id.
 * @param[in]  field       Inverse field name.
 * @param[in]  target_pos  Target record position.
 * @param[out] keys        Receives matching keys.
 * @param[in]  max         Capacity of keys.
 * @return Number of keys found.
 */
size_t hyle_source_inv_keys(
    const char *dataset_id,
    const char *field,
    uint32_t target_pos,
    const char **keys,
    size_t max);

/**
 * @brief Inverse key at an index.
 * @param[in] dataset_id  Dataset id.
 * @param[in] field       Inverse field name.
 * @param[in] target_pos  Target record position.
 * @param[in] index       Result index.
 * @return Matching key, or NULL when out of range.
 */
const char *hyle_source_inv_key_at(
    const char *dataset_id,
    const char *field,
    uint32_t target_pos,
    size_t index);

/**
 * @brief Read a string field of an item from the item-records map.
 * @param[in] hd    Item-records corm handle.
 * @param[in] id    Item id.
 * @param[in] field Field name.
 * @return Field value, or NULL when absent.
 */
const char *hyle_corm_get_field_str(
    unsigned hd,
    const char *id,
    const char *field);

/**
 * @brief Load a dataset from a DSV string.
 * @param[in] source_id Dataset id.
 * @param[in] pval      DSV payload.
 * @param[in] fhd       Fields corm handle.
 * @param[in] user      Caller data.
 * @return 0 on success, negative on failure.
 */
int hyle_source_dsv_load(
    const char *source_id,
    const char *pval,
    unsigned fhd,
    void *user);

/**
 * @brief Serialize a dataset to a DSV string.
 * @param[in] source_id Dataset id.
 * @param[in] pval      Output payload.
 * @param[in] fhd       Fields corm handle.
 * @param[in] user      Caller data.
 * @return 0 on success, negative on failure.
 */
int hyle_source_dsv_save(
    const char *source_id,
    const char *pval,
    unsigned fhd,
    void *user);

/**
 * @brief Field descriptors of a dataset.
 * @param[in]  dataset_id Dataset id.
 * @param[out] count_out  Receives the descriptor count.
 * @return Descriptor array, or NULL when unregistered.
 */
const hyle_source_desc_t *hyle_source_get_desc(
    const char *dataset_id,
    int *count_out);

/**
 * @brief Record struct size of a dataset.
 * @param[in] dataset_id Dataset id.
 * @return Record size, or 0 when unregistered.
 */
size_t hyle_source_get_record_size(
    const char *dataset_id);

/**
 * @brief Generic single-field getter callback for form parsing.
 *
 * @param[in]  name Field name.
 * @param[out] buf  Destination buffer.
 * @param[in]  sz   Capacity of buf.
 * @param[in]  user Caller data.
 * @return Copied length, or -1 when not found / unsupported.
 */
typedef int (*hyle_field_getter_fn)(const char *name, char *buf, size_t sz, void *user);

/**
 * @brief Generic multi-field getter callback for form parsing.
 * @param[in]  name Field name.
 * @param[out] buf  Destination buffer.
 * @param[in]  sz   Capacity of buf.
 * @param[in]  user Caller data.
 * @return Copied length, or -1 when not found / unsupported.
 */
typedef int (*hyle_multi_field_getter_fn)(const char *name, char *buf, size_t sz, void *user);

/**
 * @brief Parse submitted form data into an opened corm handle.
 *
 * The handle is populated with (field_name -> value) entries per the
 * dataset's schema definition.
 *
 * @param[in] def      Dataset definition.
 * @param[in] get_single Single-value field getter.
 * @param[in] get_multi  Multi-value field getter.
 * @param[in] user       Caller data passed to the getters.
 * @return Opened corm handle, or 0 on failure.
 */
unsigned hyle_source_parse_row_data_custom(
    const hyle_source_def_t *def,
    hyle_field_getter_fn get_single,
    hyle_multi_field_getter_fn get_multi,
    void *user);

/*
 * Foreign Option and Reference Resolution APIs (Entity Pattern)
 */
/**
 * @brief Display field name of a dataset.
 * @param[in] dataset_id Dataset id.
 * @param[out] out       Destination buffer.
 * @param[in] sz         Capacity of out.
 * @return 0 on success, negative on failure.
 */
int hyle_source_get_display_field(
    const char *dataset_id, char *out, size_t sz);

/**
 * @brief Display label of a row.
 * @param[in]  dataset_id    Dataset id.
 * @param[in]  row_id        Row id.
 * @param[in]  display_field Display field name.
 * @param[out] out           Destination buffer.
 * @param[in]  sz            Capacity of out.
 * @return Label string, or NULL when unresolved.
 */
const char *hyle_source_get_item_label(
    const char *dataset_id, const char *row_id, const char *display_field,
    char *out, size_t sz);

/**
 * @brief Resolve a searched option page for a picker.
 * @param[in]  dataset_id Dataset id.
 * @param[in]  q          Search term, or NULL for all.
 * @param[in]  page0      Zero-based page.
 * @param[in]  per_page   Page size.
 * @param[out] opts       Receives page options.
 * @param[in]  max        Capacity of opts.
 * @param[out] total_out  Receives total matching count.
 * @param[in]  id_buf     Scratch for option ids.
 * @param[in]  label_buf  Scratch for option labels.
 * @return Option count on success, negative on failure.
 */
int hyle_source_resolve_options(
    const char *dataset_id, const char *q, int page0, int per_page,
    hyle_option_t *opts, int max, int *total_out,
    char (*id_buf)[64], char (*label_buf)[256]);

/**
 * @brief Resolve comma-separated slugs to their option records.
 * @param[in]  dataset_id Dataset id.
 * @param[in]  comma_slugs Comma-separated slug list.
 * @param[out] out         Receives resolved options.
 * @param[in]  max         Capacity of out.
 * @param[in]  id_buf      Scratch for option ids.
 * @param[in]  label_buf   Scratch for option labels.
 * @return Option count on success, negative on failure.
 */
int hyle_source_resolve_tokens(
    const char *dataset_id, const char *comma_slugs, hyle_option_t *out,
    int max, char (*id_buf)[64], char (*label_buf)[256]);

/**
 * @brief Normalize raw token text into comma-separated slug form.
 * @param[in]  dataset_id Dataset id.
 * @param[in]  raw        Raw token text.
 * @param[out] out        Destination buffer.
 * @param[in]  out_sz     Capacity of out.
 * @return 0 on success, negative on failure.
 */
int hyle_source_normalize_tokens_to_slugs(
    const char *dataset_id, const char *raw, char *out, size_t out_sz);

/**
 * @brief Enumerate a dataset's enum-valued options.
 * @param[in]  dataset_id Dataset id.
 * @param[out] pool       Option pool storage.
 * @param[in]  pool_avail Capacity of pool.
 * @param[in]  id_buf     Scratch for option ids.
 * @param[in]  label_buf  Scratch for option labels.
 * @return Option count on success, negative on failure.
 */
int hyle_source_get_enum_options(
    const char *dataset_id, hyle_option_t *pool, int pool_avail,
    char (*id_buf)[64], char (*label_buf)[256]);

/*
 * File and Storage Utility APIs
 */
/**
 * @brief Whether an id is safe for filesystem use.
 * @param[in] id Candidate id.
 * @return Non-zero when the id contains no path separators or metacharacters.
 */
int hyle_source_is_safe_id(const char *id);

/**
 * @brief Read an entire file into a malloc'd buffer.
 * @param[in] path File path.
 * @return malloc'd contents, or NULL on failure.
 */
char *hyle_source_slurp_file(const char *path);

/**
 * @brief Write a buffer to a file (creating parents as needed).
 * @param[in] path File path.
 * @param[in] buf  Data to write.
 * @param[in] sz   Byte count.
 * @return 0 on success, negative on failure.
 */
int hyle_source_write_file(const char *path, const char *buf, size_t sz);

/**
 * @brief Recursively remove a filesystem path.
 * @param[in] path Path to remove.
 * @return 0 on success, negative on failure.
 */
int hyle_source_remove_path_recursive(const char *path);

/**
 * @brief Resolve the web document root.
 * @param[out] buf Destination buffer.
 * @param[in]  sz  Capacity of buf.
 * @return The document root, or NULL when unresolved.
 */
const char *hyle_source_resolve_doc_root(char *buf, size_t sz);

/* Internal helper shared between engine and stores */
/**
 * @brief Process multiple reference values for one field (internal).
 * @param[in] f         Field metadata.
 * @param[in] dataset_id Dataset id.
 * @param[in,out] data  Value list to process.
 * @return 0 on success, negative on failure.
 */
int hyle_source_internal_process_multi_ref(
    const hyle_source_field_t *f, const char *dataset_id, char **data);

/**
 * @brief One sync field of an ordered partition.
 */
typedef struct {
	/** Form field name prefix. */
	const char *form_field_prefix;
	/** Schema field name. */
	const char *schema_field_name;
	/** Default value when absent. */
	const char *default_value;
	/** Non-zero for the primary key field. */
	int is_primary_key;
} hyle_ordered_field_sync_t;

/**
 * @brief Sync ordered partition fields from submitted form data.
 * @param[in] source_id           Dataset id.
 * @param[in] partition_id        Partition value.
 * @param[in] amount_param        Form value holding the item count.
 * @param[in] remove_param_prefix Form param prefix marking removal slots.
 * @param[in] fields              Field sync table.
 * @param[in] n_fields            Sync table size.
 * @param[in] get_field           Form getter.
 * @param[in] user                Caller data.
 * @return 0 on success, negative on failure.
 */
int hyle_source_ordered_sync_form_custom(
    const char *source_id,
    const char *partition_id,
    const char *amount_param,
    const char *remove_param_prefix,
    const hyle_ordered_field_sync_t *fields,
    size_t n_fields,
    hyle_field_getter_fn get_field,
    void *user);

/*
 * High-level ordered partition operations (self-persisting)
 */
/**
 * @brief Read a field of an ordered-partition row.
 * @param[in] source_id    Dataset id.
 * @param[in] partition_val Partition value.
 * @param[in] index        Row index.
 * @param[in] field        Field name.
 * @return Field value, or NULL when absent.
 */
const char *hyle_source_ordered_get_field(
    const char *source_id, const char *partition_val, int index, const char *field);

/**
 * @brief Set a field of an ordered-partition row and persist.
 * @param[in] source_id    Dataset id.
 * @param[in] partition_val Partition value.
 * @param[in] index        Row index.
 * @param[in] field        Field name.
 * @param[in] value        New value.
 * @return 0 on success, negative on failure.
 */
int hyle_source_ordered_set_field(
    const char *source_id, const char *partition_val, int index, const char *field, const char *value);

/**
 * @brief Remove an ordered-partition row and persist.
 * @param[in] source_id    Dataset id.
 * @param[in] partition_val Partition value.
 * @param[in] index        Row index.
 * @return 0 on success, negative on failure.
 */
int hyle_source_ordered_remove_and_save(
    const char *source_id, const char *partition_val, int index);

/**
 * @brief Append an ordered-partition row and persist.
 * @param[in] source_id    Dataset id.
 * @param[in] partition_val Partition value.
 * @param[in] names        Field names.
 * @param[in] vals         Field values.
 * @param[in] count        Field count.
 * @return 0 on success, negative on failure.
 */
int hyle_source_ordered_append_and_save(
    const char *source_id, const char *partition_val, const char **names, const char **vals, size_t count);

/**
 * @brief Index of the first ordered row matching a field value.
 * @param[in] source_id    Dataset id.
 * @param[in] partition_val Partition value.
 * @param[in] field        Field name.
 * @param[in] val          Search value.
 * @return Row index, or -1 when absent.
 */
int hyle_source_ordered_find(
    const char *source_id, const char *partition_val, const char *field, const char *val);

/**
 * @brief Remove all rows matching a field value and persist.
 * @param[in] source_id    Dataset id.
 * @param[in] partition_val Partition value.
 * @param[in] field        Field name.
 * @param[in] val          Match value.
 * @return Removed count, or negative on failure.
 */
int hyle_source_ordered_remove_matching(
    const char *source_id, const char *partition_val, const char *field, const char *val);

/**
 * @brief Replace an ordered-partition row and persist.
 * @param[in] source_id    Dataset id.
 * @param[in] partition_val Partition value.
 * @param[in] index        Row index.
 * @param[in] names        Field names.
 * @param[in] vals         Field values.
 * @param[in] count        Field count.
 * @return 0 on success, negative on failure.
 */
int hyle_source_ordered_replace_row(
    const char *source_id, const char *partition_val, int index,
    const char **names, const char **vals, size_t count);

/**
 * @brief Per-row callback of an ordered-partition walk.
 * @param[in] index      Row index.
 * @param[in] key        Row key.
 * @param[in] fields_hd  Fields corm handle.
 * @param[in] user       Caller data.
 */
typedef void (*hyle_source_ordered_each_fn)(
    int index, const char *key, unsigned fields_hd, void *user);

/**
 * @brief Iterate an ordered partition's rows.
 * @param[in] source_id    Dataset id.
 * @param[in] partition_val Partition value.
 * @param[in] fn           Per-row callback.
 * @param[in] user         Caller data.
 * @return 0 on success, negative on failure.
 */
int hyle_source_ordered_for_each(
    const char *source_id, const char *partition_val,
    hyle_source_ordered_each_fn fn, void *user);

/*
 * High-level entity record field access
 */
/**
 * @brief Read a field of an entity item.
 * @param[in] source_id Dataset id.
 * @param[in] item_id   Item id.
 * @param[in] field     Field name.
 * @return Field value, or NULL when absent.
 */
const char *hyle_source_get_field(
    const char *source_id, const char *item_id, const char *field);

/**
 * @brief Set a field of an entity item and persist.
 * @param[in] fd        Corm file descriptor.
 * @param[in] source_id Dataset id.
 * @param[in] item_id   Item id.
 * @param[in] field     Field name.
 * @param[in] value     New value.
 * @return 0 on success, negative on failure.
 */
int hyle_source_set_field(
    int fd, const char *source_id, const char *item_id, const char *field, const char *value);

/**
 * @brief Read an integer field of an entity item.
 * @param[in] source_id Dataset id.
 * @param[in] item_id   Item id.
 * @param[in] field     Field name.
 * @param[in] def_val   Default when absent or not an integer.
 * @return Field value.
 */
int hyle_source_get_field_int(
    const char *source_id, const char *item_id, const char *field, int def_val);

/**
 * @brief Set an integer field of an entity item and persist.
 * @param[in] fd        Corm file descriptor.
 * @param[in] source_id Dataset id.
 * @param[in] item_id   Item id.
 * @param[in] field     Field name.
 * @param[in] val       New integer value.
 * @return 0 on success, negative on failure.
 */
int hyle_source_set_field_int(
    int fd, const char *source_id, const char *item_id, const char *field, int val);

/*
 * High-level relation querying (referencing items)
 */
/**
 * @brief Collect ids referencing a target through a relation field.
 * @param[in] source_dataset Source dataset id.
 * @param[in] ref_field      Relation field name.
 * @param[in] target_id      Referenced target id.
 * @param[out] ids_out       Receives matching source ids.
 * @param[in]  max           Capacity of ids_out.
 * @return Number of matches found.
 */
size_t hyle_source_find_referencing(
    const char *source_dataset,
    const char *ref_field,
    const char *target_id,
    const char **ids_out,
    size_t max);

/**
 * @brief Callback receiving one referencing source id.
 * @param[in] source_id Source dataset id.
 * @param[in] user      Caller data.
 */
typedef void (*hyle_source_ref_cb_t)(const char *source_id, void *user);

/**
 * @brief Visit all source ids referencing a target through a field.
 * @param[in] source_dataset Source dataset id.
 * @param[in] ref_field      Relation field name.
 * @param[in] target_id      Referenced target id.
 * @param[in] cb             Per-source callback.
 * @param[in] user           Caller data.
 * @return Number of referencing sources visited.
 */
size_t hyle_source_for_each_referencing(
    const char *source_dataset,
    const char *ref_field,
    const char *target_id,
    hyle_source_ref_cb_t cb,
    void *user);

#endif
