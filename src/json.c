#include "hyle-source/hyle_source.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <ttypt/qmap.h>

static json_object *hyle_source_build_string_array(const char *input)
{
	const char *p, *start;
	char *field_val;

	json_object *ja = json_object_new_array();
	if (!ja)
		return NULL;
	if (input && input[0]) {
		p = input;
		start = p;
		while (*p) {
			if (*p == '\n') {
				field_val = strndup(start, (size_t)(p - start));
				if (field_val) {
					json_object_array_add(
					        ja, json_object_new_string(
					                    field_val));
					free(field_val);
				}
				p++;
				start = p;
			} else {
				p++;
			}
		}
		if (p > start) {
			field_val = strndup(start, (size_t)(p - start));
			if (field_val) {
				json_object_array_add(
				        ja, json_object_new_string(field_val));
				free(field_val);
			}
		}
	}
	return ja;
}

static json_object *hyle_source_build_inverse_array(
        const hyle_source_def_t *def, const hyle_source_field_t *field,
        const char *item_id)
{
	if (!field->target_source || !field->inverse_name)
		return json_object_new_array();

	const hyle_source_def_t *target = hyle_source_find(field->target_source);
	if (!target || !target->fields_hd)
		return json_object_new_array();

	uint32_t pos = qmap_pos(def->fields_hd, item_id);
	if (pos == UINT32_MAX)
		return json_object_new_array();

	uint32_t inv_buf[256];
	size_t count = qmap_inv_get(
	        target->fields_hd, field->inverse_name, pos, inv_buf, 256);

	json_object *ja = json_object_new_array();
	if (!ja)
		return json_object_new_array();

	for (size_t i = 0; i < count; i++) {
		const char *key = qmap_get_key(target->fields_hd, inv_buf[i]);
		if (key) {
			json_object_array_add(ja, json_object_new_string(key));
		}
	}
	return ja;
}

int hyle_source_build_item_json(
        const hyle_source_def_t *def, const char *item_id, json_object **out_jo)
{
	json_object *jo = json_object_new_object();
	if (!jo)
		return -1;

	json_object_object_add(jo, "id", json_object_new_string(item_id));

	for (size_t i = 0; i < def->field_count; i++) {
		const hyle_source_field_t *f = &def->fields[i];
		if (strcmp(f->name, "id") == 0)
			continue;

		const char *val =
		        qmap_field_get(def->fields_hd, item_id, f->name);

		switch (f->type) {
		case HYLE_FIELD_STRING:
			if (val)
				json_object_object_add(
				        jo, f->name,
				        json_object_new_string(val));
			break;
		case HYLE_FIELD_NULLABLE_STRING:
			if (val && val[0])
				json_object_object_add(
				        jo, f->name,
				        json_object_new_string(val));
			break;
		case HYLE_FIELD_INT:
			if (val)
				json_object_object_add(
				        jo, f->name,
				        json_object_new_int(atoi(val)));
			break;
		case HYLE_FIELD_BOOL:
			if (val)
				json_object_object_add(
				        jo, f->name,
				        json_object_new_boolean(
				                strcmp(val, "1") == 0 ||
				                strcmp(val, "true") == 0));
			break;
		case HYLE_FIELD_REFERENCE:
			if (val)
				json_object_object_add(
				        jo, f->name,
				        json_object_new_string(val));
			break;
		case HYLE_FIELD_MULTI_REFERENCE: {
			json_object *arr = hyle_source_build_string_array(val);
			json_object_object_add(
			        jo, f->name,
			        arr ? arr : json_object_new_array());
			break;
		}
		case HYLE_FIELD_INVERSE: {
			json_object *arr = hyle_source_build_inverse_array(
			        def, f, item_id);
			json_object_object_add(
			        jo, f->name,
			        arr ? arr : json_object_new_array());
			break;
		}
		case HYLE_FIELD_DERIVED:
		default:
			break;
		}
	}

	*out_jo = jo;
	return 0;
}

static void hyle_source_resolve_ref_display(
        json_object *jo, const hyle_source_def_t *def,
        const hyle_source_field_t *f, const char *item_id)
{
	char result[4096] = { 0 };

	if (hyle_source_resolve_ref_display_str(
	            def->id, item_id, f->name, result, sizeof(result)) != 0)
		return;
	if (!result[0])
		return;

	json_object_object_del(jo, f->name);
	json_object_object_add(jo, f->name, json_object_new_string(result));
}

int hyle_source_build_state_json(
        const char *dataset_id, const char *item_id,
        const hyle_source_state_field_t *specs, json_object **out)
{
	const hyle_source_def_t *def;
	json_object *jo;

	if (out)
		*out = NULL;

	def = hyle_source_find(dataset_id);
	if (!def || !item_id || !item_id[0])
		return -1;

	if (!qmap_get(def->source_hd, item_id)) {
		if (hyle_source_refresh_row(0, dataset_id, item_id) != 0)
			return -1;
	}

	if (hyle_source_build_item_json(def, item_id, &jo) != 0)
		return -1;

	for (const hyle_source_state_field_t *s = specs; s && s->name; s++) {
		switch (s->kind) {
		case HYLE_SF_EXCLUDE:
			json_object_object_del(jo, s->name);
			break;
		case HYLE_SF_REF_DISPLAY: {
			const hyle_source_field_t *f = NULL;
			for (size_t i = 0; i < def->field_count; i++) {
				if (strcmp(def->fields[i].name, s->name) == 0) {
					f = &def->fields[i];
					break;
				}
			}
			if (f && (f->type == HYLE_FIELD_MULTI_REFERENCE ||
			          f->type == HYLE_FIELD_REFERENCE) &&
			    f->target_source)
				hyle_source_resolve_ref_display(jo, def, f, item_id);
			break;
		}
		default:
			break;
		}
	}

	*out = jo;
	return 0;
}

int hyle_source_state_overlay(json_object *jo, const hyle_source_state_kv_t *kvs)
{
	if (!jo || !kvs)
		return 0;
	for (const hyle_source_state_kv_t *kv = kvs; kv->key; kv++) {
		if (kv->is_int)
			json_object_object_add(
			        jo, kv->key, json_object_new_int(kv->int_val));
		else
			json_object_object_add(
			        jo, kv->key,
			        json_object_new_string(
			                kv->str_val ? kv->str_val : ""));
	}
	return 0;
}

int hyle_source_overlay_from_desc(
        json_object *jo,
        const void *state,
        const hyle_source_desc_t *fields,
        int int_kind,
        int str_kind)
{
	hyle_source_state_kv_t kvs[32];
	int n = 0;
	for (const hyle_source_desc_t *f = fields; f->key && n < 31; f++) {
		if (f->kind == int_kind) {
			kvs[n].key = f->key;
			kvs[n].is_int = 1;
			kvs[n].int_val =
			        *(int *)((const char *)state + f->offset);
			kvs[n].str_val = NULL;
			n++;
		} else if (f->kind == str_kind) {
			kvs[n].key = f->key;
			kvs[n].is_int = 0;
			kvs[n].int_val = 0;
			kvs[n].str_val =
			        (const char *)((const char *)state + f->offset);
			n++;
		}
	}
	kvs[n].key = NULL;
	hyle_source_state_overlay(jo, kvs);
	return 0;
}

json_object *hyle_source_overlay_array(
        const void *items, int count, size_t elem_size,
        const hyle_source_desc_t *fields,
        int int_kind, int str_kind)
{
	json_object *ja = json_object_new_array();
	if (!ja)
		return NULL;
	for (int i = 0; i < count; i++) {
		const void *item = (const char *)items + (size_t)i * elem_size;
		json_object *jo = json_object_new_object();
		if (!jo)
			continue;
		hyle_source_overlay_from_desc(
		        jo, item, fields, int_kind, str_kind);
		json_object_array_add(ja, jo);
	}
	return ja;
}
