#ifndef LIBHYLE_SOURCE_H
#define LIBHYLE_SOURCE_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <json-c/json.h>
#include <hyle/field.h>
#include <hyle/schema.h>
#include <hyle/picker.h>
#include "store.h"

typedef enum {
	HYLE_SOURCE_ACCESS_PUBLIC = 0,
	HYLE_SOURCE_ACCESS_LOGIN,
} hyle_source_access_policy_t;

typedef enum {
	HYLE_SOURCE_ACCESS_RESULT_ALLOW = 0,
	HYLE_SOURCE_ACCESS_RESULT_UNAUTHORIZED,
	HYLE_SOURCE_ACCESS_RESULT_FORBIDDEN,
} hyle_source_access_result_t;

#define HYLE_SOURCE_FIELD_KIND_INVERSE HYLE_KIND_INVERSE

typedef struct {
	const char *name;
	const char *file;
	hyle_field_type_t type;
	int writable;
	const char *target_source;
	const char *inverse_name;
	int required;
	int64_t min;
	int64_t max;
	size_t min_length;
	size_t max_length;
	const char *pattern;
	const char *filter_style;
	const char *filter_mode;
	const char *derive_key;
} hyle_source_field_t;

typedef struct {
	const char *name;
	const char *label;
} hyle_source_list_field_t;

typedef struct {
	const char *display_name;
	const hyle_source_list_field_t *fields;
	size_t field_count;
	const char *default_sort;
	const char *content_field;
	const char *content_label;
	const char *content_placeholder;
} hyle_source_list_view_t;

/* Framework-neutral unified descriptor aliases */
typedef hyle_schema_desc_t hyle_source_desc_t;
typedef hyle_schema_desc_t source_desc_t;

typedef struct hyle_source_def_s {
	const char *id;
	const char *key_field;
	const char *items_path;
	hyle_source_access_policy_t access_policy;
	const hyle_source_field_t *fields;
	size_t field_count;
	unsigned source_hd;
	unsigned fields_hd;
	unsigned schema_hd;
	uint32_t record_id;
	unsigned flags;
	const hyle_source_list_view_t *list_view;
	const hyle_source_desc_t *defs;
	int def_count;
	size_t record_size;
	char display_field[64];
	void *user;
	hyle_source_store_t store;
} hyle_source_def_t;

typedef int (*hyle_source_each_cb_t)(const hyle_source_def_t *, void *);

#define HYLE_SOURCE_FLAG_VOLATILE 64u
#define HYLE_SOURCE_FLAG_CREATABLE 128u
#define HYLE_SOURCE_ERR_VALIDATION -2

/* ── State JSON builder ─────────────────────────────────────────── */

typedef enum {
	HYLE_SF_RECORD,
	HYLE_SF_EXCLUDE,
	HYLE_SF_REF_DISPLAY,
} hyle_source_state_kind_t;

typedef struct {
	const char *name;
	hyle_source_state_kind_t kind;
} hyle_source_state_field_t;

typedef struct {
	const char *key;
	int is_int;
	int int_val;
	const char *str_val;
} hyle_source_state_kv_t;

typedef struct {
	const char *key;
	char *dest;
	size_t dest_size;
} hyle_json_str_map_t;

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

int hyle_source_clear_inverse_refs(
    int fd,
    const char *dataset_id,
    const char *item_id);

int hyle_source_def_to_qmap(
    const hyle_source_desc_t *defs, int count, void *out);

int hyle_source_def_to_source_fields(
    const hyle_source_desc_t *defs, int count, void *out);

int hyle_source_def_to_meta_fields(
    const hyle_source_desc_t *defs, int count,
    const void *record, void *out);

int hyle_source_build_state_specs(
    const hyle_source_desc_t *fields,
    hyle_source_state_field_t *specs,
    int max_specs);

hyle_source_def_t *hyle_source_find(const char *dataset_id);
int hyle_source_is_creatable(const char *dataset_id);

int hyle_source_item_exists(
    const char *dataset_id,
    const char *item_id);

int hyle_source_register_def(const hyle_source_def_t *def);

int hyle_source_refresh_row(
    int fd, const char *dataset_id, const char *id);

int hyle_source_validate_row(
    const hyle_source_def_t *def, unsigned data_handle, char **json_errors_out);

int hyle_source_update_item(
    int fd, const char *dataset_id,
    const char *id, unsigned data_handle);

int hyle_source_delete_item(
    int fd, const hyle_source_def_t *def, const char *item_id);

int hyle_ref_field_register(
    const char *dataset_id, const char *field_name);

int hyle_source_for_each(hyle_source_each_cb_t cb, void *user);

unsigned hyle_source_query_dataset(
    const char *dataset_id,
    const char *query_str);

unsigned hyle_source_get_data_hd(const char *dataset_id);
unsigned hyle_source_get_fields_hd(const char *dataset_id);
unsigned hyle_source_get_schema_hd(const char *dataset_id);
const hyle_source_list_view_t *hyle_source_get_list_view(
    const char *dataset_id);

int hyle_source_build_item_json(
    const hyle_source_def_t *def,
    const char *item_id,
    json_object **out);

int hyle_source_build_state_json(
    const char *dataset_id,
    const char *item_id,
    const hyle_source_state_field_t *specs,
    json_object **out);

int hyle_source_state_overlay(
    json_object *jo,
    const hyle_source_state_kv_t *kvs);

int hyle_source_overlay_from_desc(
    json_object *jo,
    const void *state,
    const hyle_source_desc_t *fields,
    int int_kind,
    int str_kind);

json_object *hyle_source_overlay_array(
    const void *items, int count, size_t elem_size,
    const hyle_source_desc_t *fields,
    int int_kind, int str_kind);

int hyle_source_resolve_ref_display_str(
    const char *dataset_id,
    const char *item_id,
    const char *field_name,
    char *out, size_t out_sz);

int hyle_source_resolve_meta_display(
    const char *dataset_id,
    const char *item_id,
    const hyle_source_desc_t *fields,
    int count,
    void *state);

int hyle_source_meta_read(
    const char *path,
    const hyle_source_desc_t *fields,
    int count,
    void *record,
    size_t record_size);

int hyle_source_meta_write(
    const char *path,
    const hyle_source_desc_t *fields,
    int count,
    const void *record);

uint32_t hyle_source_setup(
    const char *source_id,
    const char *key_field,
    size_t record_size,
    const char *items_path,
    const hyle_source_desc_t *defs,
    int field_count,
    unsigned flags,
    const hyle_source_list_view_t *list_view);

size_t hyle_source_inv_keys(
    const char *dataset_id,
    const char *field,
    uint32_t target_pos,
    const char **keys,
    size_t max);

const char *hyle_source_inv_key_at(
    const char *dataset_id,
    const char *field,
    uint32_t target_pos,
    size_t index);

const char *hyle_qmap_get_field_str(
    unsigned hd,
    const char *id,
    const char *field);

int hyle_source_dsv_load(
    const char *source_id,
    const char *pval,
    unsigned fhd,
    void *user);

int hyle_source_dsv_save(
    const char *source_id,
    const char *pval,
    unsigned fhd,
    void *user);

const hyle_source_desc_t *hyle_source_get_desc(
    const char *dataset_id,
    int *count_out);

size_t hyle_source_get_record_size(
    const char *dataset_id);

/*
 * Generic field getter callback for form parsing.
 * Takes field name, buffer, and buffer size.
 * Returns copied length or -1 if not found / unsupported.
 */
typedef int (*hyle_field_getter_fn)(const char *name, char *buf, size_t sz, void *user);
typedef int (*hyle_multi_field_getter_fn)(const char *name, char *buf, size_t sz, void *user);

/*
 * Parse submitted form data according to the schema definition and return
 * an opened qmap handle populated with (field_name -> value) entries.
 */
unsigned hyle_source_parse_row_data_custom(
    const hyle_source_def_t *def,
    hyle_field_getter_fn get_single,
    hyle_multi_field_getter_fn get_multi,
    void *user);

/*
 * Foreign Option and Reference Resolution APIs (Entity Pattern)
 */
int hyle_source_get_display_field(
    const char *dataset_id, char *out, size_t sz);

const char *hyle_source_get_item_label(
    const char *dataset_id, const char *row_id, const char *display_field,
    char *out, size_t sz);

int hyle_source_resolve_options(
    const char *dataset_id, const char *q, int page0, int per_page,
    hyle_option_t *opts, int max, int *total_out,
    char (*id_buf)[64], char (*label_buf)[256]);

int hyle_source_resolve_tokens(
    const char *dataset_id, const char *comma_slugs, hyle_option_t *out,
    int max, char (*id_buf)[64], char (*label_buf)[256]);

int hyle_source_normalize_tokens_to_slugs(
    const char *dataset_id, const char *raw, char *out, size_t out_sz);

int hyle_source_get_enum_options(
    const char *dataset_id, hyle_option_t *pool, int pool_avail,
    char (*id_buf)[64], char (*label_buf)[256]);

/*
 * File and Storage Utility APIs
 */
int hyle_source_is_safe_id(const char *id);
char *hyle_source_slurp_file(const char *path);
int hyle_source_write_file(const char *path, const char *buf, size_t sz);
int hyle_source_remove_path_recursive(const char *path);
const char *hyle_source_resolve_doc_root(char *buf, size_t sz);

/* Internal helper shared between engine and stores */
int hyle_source_internal_process_multi_ref(
    const hyle_source_field_t *f, const char *dataset_id, char **data);

typedef struct {
	const char *form_field_prefix;
	const char *schema_field_name;
	const char *default_value;
	int is_primary_key;
} hyle_ordered_field_sync_t;

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
const char *hyle_source_ordered_get_field(
    const char *source_id, const char *partition_val, int index, const char *field);

int hyle_source_ordered_set_field(
    const char *source_id, const char *partition_val, int index, const char *field, const char *value);

int hyle_source_ordered_remove_and_save(
    const char *source_id, const char *partition_val, int index);

int hyle_source_ordered_append_and_save(
    const char *source_id, const char *partition_val, const char **names, const char **vals, size_t count);

int hyle_source_ordered_find(
    const char *source_id, const char *partition_val, const char *field, const char *val);

int hyle_source_ordered_remove_matching(
    const char *source_id, const char *partition_val, const char *field, const char *val);

int hyle_source_ordered_replace_row(
    const char *source_id, const char *partition_val, int index,
    const char **names, const char **vals, size_t count);

typedef void (*hyle_source_ordered_each_fn)(
    int index, const char *key, unsigned fields_hd, void *user);

int hyle_source_ordered_for_each(
    const char *source_id, const char *partition_val,
    hyle_source_ordered_each_fn fn, void *user);

/*
 * High-level entity record field access
 */
const char *hyle_source_get_field(
    const char *source_id, const char *item_id, const char *field);

int hyle_source_set_field(
    int fd, const char *source_id, const char *item_id, const char *field, const char *value);

int hyle_source_get_field_int(
    const char *source_id, const char *item_id, const char *field, int def_val);

int hyle_source_set_field_int(
    int fd, const char *source_id, const char *item_id, const char *field, int val);

/*
 * High-level relation querying (referencing items)
 */
size_t hyle_source_find_referencing(
    const char *source_dataset,
    const char *ref_field,
    const char *target_id,
    const char **ids_out,
    size_t max);

typedef void (*hyle_source_ref_cb_t)(const char *source_id, void *user);

size_t hyle_source_for_each_referencing(
    const char *source_dataset,
    const char *ref_field,
    const char *target_id,
    hyle_source_ref_cb_t cb,
    void *user);

#endif
