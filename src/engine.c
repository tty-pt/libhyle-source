#include "hyle-source/hyle_source.h"
#include "hyle-source/store.h"
#include "source_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

#include <ttypt/corm.h>
#include <stoma/stoma.h>
#include <hyle/hyle.h>
#include <hyle/registry.h>

static void str_trim(char *s)
{
	if (!s || !s[0])
		return;
	char *p = s;
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
		p++;
	if (p > s)
		memmove(s, p, strlen(p) + 1);
	size_t len = strlen(s);
	while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
	                   s[len - 1] == '\r' || s[len - 1] == '\n'))
	{
		s[--len] = '\0';
	}
}


hyle_source_def_t *hyle_source_find(const char *dataset_id)
{
	if (!dataset_id || !dataset_id[0])
		return NULL;
	return (hyle_source_def_t *)hyle_registry_get_user(dataset_id);
}

int hyle_source_is_creatable(const char *dataset_id)
{
	if (!dataset_id || !dataset_id[0])
		return 0;
	const hyle_source_def_t *def = hyle_source_find(dataset_id);
	if (!def)
		return 0;
	if (def->flags & HYLE_SOURCE_FLAG_CREATABLE)
		return 1;
	/* Tag/catalog dictionary datasets with key_field and without a custom list_view are creatable */
	if (def->key_field && def->key_field[0] && !def->list_view)
		return 1;
	return 0;
}

int hyle_source_item_exists(
        const char *dataset_id,
        const char *item_id)
{
	const hyle_source_def_t *def;

	if (!item_id || !item_id[0])
		return 0;
	def = hyle_source_find(dataset_id);
	if (!def || !def->fields_hd)
		return 0;
	return corm_pos(def->fields_hd, item_id) != CM_MISS;
}

static void resolve_ref_append(
        char *out, size_t *rpos, size_t out_sz, const hyle_source_def_t *target,
        const char *token)
{
	const char *target_id = NULL;
	const char *display = NULL;
	char *end;
	uint32_t pos;
	size_t dlen;

	pos = (uint32_t)strtoul(token, &end, 10);
	if (target->fields_hd && end != token && *end == '\0')
		target_id = corm_get_key(target->fields_hd, pos);
	if (!target_id)
		target_id = token;

	display =
	        corm_field_get(target->fields_hd, target_id, target->key_field);
	if (!display || !display[0])
		display = target_id;
	if (!display || !display[0])
		return;

	if (*rpos > 0 && *rpos < out_sz - 1) {
		out[(*rpos)++] = '\n';
	}

	dlen = strlen(display);
	if (*rpos + dlen < out_sz) {
		memcpy(out + *rpos, display, dlen);
		*rpos += dlen;
	}
}

int hyle_source_resolve_ref_display_str(
        const char *dataset_id,
        const char *item_id,
        const char *field_name,
        char *out, size_t out_sz)
{
	const hyle_source_def_t *def;
	const hyle_source_field_t *f = NULL;
	const char *val;
	const hyle_source_def_t *target;

	if (out && out_sz > 0)
		out[0] = '\0';
	if (!dataset_id || !item_id || !field_name || !out || out_sz == 0)
		return -1;

	def = hyle_source_find(dataset_id);
	if (!def || !def->fields_hd)
		return -1;

	for (size_t i = 0; i < def->field_count; i++) {
		if (strcmp(def->fields[i].name, field_name) == 0) {
			f = &def->fields[i];
			break;
		}
	}
	if (!f || (f->type != HYLE_FIELD_MULTI_REFERENCE && f->type != HYLE_FIELD_REFERENCE) || !f->target_source)
		return -1;

	val = corm_field_get(def->fields_hd, item_id, field_name);
	if (!val || !val[0])
		return 0;

	target = hyle_source_find(f->target_source);
	if (!target || !target->fields_hd || !target->key_field)
		return -1;

	size_t rpos = 0;
	const char *p = val;
	const char *start = p;

	while (*p) {
		if (*p == '\n') {
			char id_buf[256];
			size_t len = (size_t)(p - start);
			if (len >= sizeof(id_buf))
				len = sizeof(id_buf) - 1;
			memcpy(id_buf, start, len);
			id_buf[len] = '\0';

			resolve_ref_append(out, &rpos, out_sz, target, id_buf);
			p++;
			start = p;
		} else {
			p++;
		}
	}
	if (p > start) {
		char id_buf[256];
		size_t len = (size_t)(p - start);
		if (len >= sizeof(id_buf))
			len = sizeof(id_buf) - 1;
		memcpy(id_buf, start, len);
		id_buf[len] = '\0';

		resolve_ref_append(out, &rpos, out_sz, target, id_buf);
	}
	if (rpos < out_sz)
		out[rpos] = '\0';
	else if (out_sz > 0)
		out[out_sz - 1] = '\0';

	return 0;
}

int hyle_source_resolve_meta_display(
        const char *dataset_id,
        const char *item_id,
        const hyle_source_desc_t *fields,
        int count,
        void *state)
{
	int resolved = 0;

	if (!dataset_id || !item_id || !fields || !state)
		return -1;

	for (int i = 0; i < count; i++) {
		if (fields[i].type != HYLE_FIELD_MULTI_REFERENCE &&
		    fields[i].type != HYLE_FIELD_REFERENCE &&
		    fields[i].kind != HYLE_SF_REF_DISPLAY)
			continue;
		if (!fields[i].key || !fields[i].size)
			continue;
		char buf[4096] = { 0 };
		if (hyle_source_resolve_ref_display_str(
		            dataset_id, item_id, fields[i].key, buf,
		            sizeof(buf)) == 0 &&
		    buf[0])
		{
			snprintf(
			        (char *)state + fields[i].offset,
			        fields[i].size, "%s", buf);
			resolved++;
		}
	}
	return resolved > 0 ? 0 : -1;
}

static void
source_ensure_entity(const char *ref_source, const char *display_name)
{
	hyle_source_def_t *target;
	char slug[64];

	if (!ref_source || !display_name || !display_name[0])
		return;
	target = hyle_source_find(ref_source);
	if (!target)
		return;
	source_util_slugify(display_name, strlen(display_name), slug, sizeof(slug));
	if (!slug[0])
		return;

	const char *fname =
	        target->key_field ? target->key_field : "name";
	if (target->fields_hd) {
		const char *existing =
		        corm_field_get(target->fields_hd, slug, fname);
		if (existing && existing[0])
			return;
	}

	if (target->store.ops && target->store.ops->put_field) {
		target->store.ops->put_field(
		        (hyle_source_store_t *)&target->store, target, slug, fname, display_name);
	}
	hyle_source_refresh_row(0, target->id, slug);
}

static void
source_ensure_tokens(const char *target_source, const char *val)
{
	if (!target_source || !val || !val[0])
		return;
	char buf[4096];
	char *tok, *saveptr;
	snprintf(buf, sizeof(buf), "%s", val);
	tok = strtok_r(buf, "\r\n", &saveptr);
	while (tok) {
		str_trim(tok);
		if (tok[0])
			source_ensure_entity(target_source, tok);
		tok = strtok_r(NULL, "\r\n", &saveptr);
	}
}

typedef struct {
	char dataset_id[64];
	char field_name[64];
} ref_reg_t;

static ref_reg_t g_ref_regs[64];
static size_t g_num_ref_regs = 0;

int hyle_ref_field_register(
        const char *dataset_id, const char *field_name)
{
	if (!dataset_id || !field_name ||
	    g_num_ref_regs >= sizeof(g_ref_regs) / sizeof(g_ref_regs[0]))
		return -1;

	for (size_t i = 0; i < g_num_ref_regs; i++) {
		if (strcmp(g_ref_regs[i].dataset_id, dataset_id) == 0 &&
		    strcmp(g_ref_regs[i].field_name, field_name) == 0)
			return 0;
	}

	snprintf(
	        g_ref_regs[g_num_ref_regs].dataset_id,
	        sizeof(g_ref_regs[g_num_ref_regs].dataset_id), "%s",
	        dataset_id);
	snprintf(
	        g_ref_regs[g_num_ref_regs].field_name,
	        sizeof(g_ref_regs[g_num_ref_regs].field_name), "%s",
	        field_name);
	g_num_ref_regs++;
	return 0;
}

static int ref_is_registered(const char *dataset_id, const char *field_name)
{
	for (size_t i = 0; i < g_num_ref_regs; i++) {
		if (strcmp(g_ref_regs[i].dataset_id, dataset_id) == 0 &&
		    strcmp(g_ref_regs[i].field_name, field_name) == 0)
			return 1;
	}
	return 0;
}

static int ref_normalize(
        const char *dataset_id, const char *field_name, char **data,
        size_t *len)
{
	if (!ref_is_registered(dataset_id, field_name))
		return 0;
	if (!data || !*data || !(*data)[0])
		return 0;

	char result[4096] = { 0 };
	char *buf = strdup(*data);
	if (!buf)
		return 0;

	char *tok, *saveptr;
	tok = strtok_r(buf, "\r\n", &saveptr);
	int first = 1;

	while (tok) {
		str_trim(tok);
		if (tok[0]) {
			char id_norm[256];
			size_t rlen;
			source_util_slugify(
			        tok, strlen(tok), id_norm, sizeof(id_norm));
			rlen = strlen(result);
			snprintf(
			        result + rlen, sizeof(result) - rlen, "%s%s",
			        first ? "" : "\n", id_norm);
			first = 0;
		}
		tok = strtok_r(NULL, "\r\n", &saveptr);
	}
	free(buf);

	if (result[0]) {
		free(*data);
		*data = strdup(result);
		if (*data && len)
			*len = strlen(*data);
	}

	return 0;
}

int hyle_source_internal_process_multi_ref(
        const hyle_source_field_t *f, const char *dataset_id, char **data)
{
	if (!f || !dataset_id || !data || !*data || !(*data)[0])
		return 0;
	if ((f->type != HYLE_FIELD_MULTI_REFERENCE &&
	     f->type != HYLE_FIELD_REFERENCE) ||
	    !f->target_source)
		return 0;
	source_ensure_tokens(f->target_source, *data);
	size_t len = strlen(*data);
	return ref_normalize(dataset_id, f->name, data, &len);
}

int hyle_source_for_each(hyle_source_each_cb_t cb, void *user)
{
	size_t count;
	size_t i;

	if (!cb)
		return -1;
	count = hyle_registry_count();
	for (i = 0; i < count; i++) {
		const char *sid = hyle_registry_id_at(i);
		hyle_source_def_t *def;

		if (!sid)
			continue;
		def = (hyle_source_def_t *)hyle_registry_get_user(sid);
		if (!def)
			continue;
		if (cb(def, user) != 0)
			return 1;
	}
	return 0;
}

int hyle_source_delete_item(
        int fd,
        const hyle_source_def_t *def,
        const char *item_id)
{
	(void)fd;
	if (!def || !item_id)
		return -1;
	if (def->store.ops && def->store.ops->del)
		def->store.ops->del((hyle_source_store_t *)&def->store, def, item_id);
	hyle_registry_del(def->id, item_id);
	return 0;
}

struct clear_inv_ctx {
	const hyle_source_def_t *def;
	uint32_t item_pos;
	const char *item_id;
};

static int clear_inv_refs_cb(const hyle_source_def_t *target, void *user)
{
	struct clear_inv_ctx *ctx = user;
	if (target == ctx->def)
		return 0;
	if (!target->record_id || !target->fields_hd)
		return 0;

	for (size_t i = 0; i < target->field_count; i++) {
		const hyle_source_field_t *f = &target->fields[i];
		if (f->type != HYLE_FIELD_REFERENCE &&
		    f->type != HYLE_FIELD_MULTI_REFERENCE)
			continue;
		if (!f->target_source)
			continue;
		if (strcmp(f->target_source, ctx->def->id) != 0)
			continue;

		uint32_t inv_buf[256];
		size_t count = corm_inv_get(
		        target->fields_hd, f->name, ctx->item_pos, inv_buf,
		        256);
		for (size_t j = 0; j < count; j++) {
			const char *ref_key =
			        corm_get_key(target->fields_hd, inv_buf[j]);
			if (!ref_key)
				continue;

			const char *cur = corm_field_get(
			        target->fields_hd, ref_key, f->name);
			char remaining[8192];
			size_t rem_len = 0;
			int first = 1;

			remaining[0] = '\0';
			if (cur && cur[0]) {
				char buf[8192];
				char *tok, *sv;

				snprintf(buf, sizeof(buf), "%s", cur);
				tok = strtok_r(buf, "\r\n", &sv);
				while (tok) {
					size_t len;
					str_trim(tok);
					len = strlen(tok);
					if (len > 0 &&
					    strcmp(tok, ctx->item_id) != 0)
					{
						if (!first)
							remaining[rem_len++] =
							        '\n';
						memcpy(remaining + rem_len, tok,
						       len);
						rem_len += len;
						first = 0;
					}
					tok = strtok_r(NULL, "\r\n", &sv);
				}
				remaining[rem_len] = '\0';
			}

			corm_field_put(
			        target->fields_hd, ref_key, f->name, remaining);

			if (target->store.ops && target->store.ops->put_field) {
				target->store.ops->put_field(
				        (hyle_source_store_t *)&target->store,
				        target, ref_key, f->name, remaining);
			}
		}
	}
	return 0;
}

int hyle_source_clear_inverse_refs(
        int fd,
        const char *dataset_id,
        const char *item_id)
{
	(void)fd;
	const hyle_source_def_t *def = hyle_source_find(dataset_id);
	if (!def || !item_id || !def->fields_hd)
		return 0;

	uint32_t item_pos = corm_pos(def->fields_hd, item_id);
	if (item_pos == UINT32_MAX)
		return 0;

	struct clear_inv_ctx ctx = {
		.def = def,
		.item_pos = item_pos,
		.item_id = item_id,
	};
	hyle_source_for_each(clear_inv_refs_cb, &ctx);
	return 0;
}

int hyle_source_refresh_row(
        int fd, const char *dataset_id, const char *id)
{
	(void)fd;
	const hyle_source_def_t *def = hyle_source_find(dataset_id);
	if (!def)
		return -1;
	if (def->store.ops && def->store.ops->load)
		return def->store.ops->load(
		        (hyle_source_store_t *)&def->store, def, id, NULL);
	return -1;
}

int hyle_source_validate_row(
        const hyle_source_def_t *def, unsigned data_handle, char **json_errors_out)
{
	if (!def)
		return -1;
	size_t n = def->field_count;
	hyle_field_t *hfields = malloc(n * sizeof(hyle_field_t));
	const char **values = malloc(n * sizeof(const char *));
	if (!hfields || !values) {
		free(hfields);
		free(values);
		return -1;
	}

	for (size_t i = 0; i < n; i++) {
		const hyle_source_field_t *sf = &def->fields[i];
		hfields[i].name = sf->name;
		hfields[i].type = sf->type;
		hfields[i].writable = sf->writable;
		hfields[i].target_source = sf->target_source;
		hfields[i].inverse_name = sf->inverse_name;
		hfields[i].required = sf->required;
		hfields[i].min = sf->min;
		hfields[i].max = sf->max;
		hfields[i].min_length = sf->min_length;
		hfields[i].max_length = sf->max_length;
		hfields[i].pattern = sf->pattern;
		hfields[i].searchable = 0;
		hfields[i].combine = 0;
		values[i] = corm_get(data_handle, sf->name);
	}

	hyle_purify_error_t *errs = NULL;
	size_t nerr = 0;
	int rc = hyle_purify_row(hfields, n, values, &errs, &nerr);

	free(hfields);
	free(values);

	if (rc != 0) {
		if (json_errors_out && errs && nerr > 0) {
			json_object *j_errors = json_object_new_array();
			for (size_t j = 0; j < nerr; j++) {
				json_object *j_err = json_object_new_object();
				json_object_object_add(
				        j_err, "field",
				        json_object_new_string(errs[j].field ? errs[j].field : ""));
				json_object_object_add(
				        j_err, "rule",
				        json_object_new_string(errs[j].rule ? errs[j].rule : ""));
				json_object_object_add(
				        j_err, "message",
				        json_object_new_string(errs[j].message ? errs[j].message : ""));
				json_object_array_add(j_errors, j_err);
			}
			json_object *j_root = json_object_new_object();
			json_object_object_add(j_root, "errors", j_errors);
			const char *s = json_object_to_json_string(j_root);
			*json_errors_out = strdup(s ? s : "{}");
			json_object_put(j_root);
		}
		if (errs)
			hyle_purify_errors_free(errs, nerr);
		return 1;
	}
	return 0;
}

int hyle_source_update_item(
        int fd,
        const char *dataset_id,
        const char *id,
        unsigned data_handle)
{
	(void)fd;
	const hyle_source_def_t *def = hyle_source_find(dataset_id);
	if (!def)
		return -1;
	if (hyle_source_validate_row(def, data_handle, NULL))
		return HYLE_SOURCE_ERR_VALIDATION;

	if (!id || !id[0] || !source_util_is_safe_id(id))
		return -1;

	if (def->store.ops && def->store.ops->put) {
		for (size_t i = 0; i < def->field_count; i++) {
			const hyle_source_field_t *f = &def->fields[i];
			if ((f->type != HYLE_FIELD_MULTI_REFERENCE &&
			     f->type != HYLE_FIELD_REFERENCE) ||
			    !f->target_source)
				continue;
			const char *val = corm_get(data_handle, f->name);
			source_ensure_tokens(f->target_source, val);
		}
		if (def->store.ops->put(
		            (hyle_source_store_t *)&def->store, def, id,
		            data_handle) != 0)
			return -1;
	}

	int result = hyle_source_refresh_row(fd, dataset_id, id);

	if (result == 0) {
		for (size_t i = 0; i < def->field_count; i++) {
			const hyle_source_field_t *f = &def->fields[i];
			if ((f->type != HYLE_FIELD_MULTI_REFERENCE &&
			     f->type != HYLE_FIELD_REFERENCE) ||
			    !f->target_source || !f->file)
				continue;
			char display[8192] = { 0 };
			if (hyle_source_resolve_ref_display_str(
			            def->id, id, f->name, display,
			            sizeof(display)) != 0)
				continue;
			if (!display[0])
				continue;
			if (def->store.ops && def->store.ops->put_field)
				def->store.ops->put_field(
				        (hyle_source_store_t *)&def->store, def, id,
				        f->file, display);
		}
	}

	return result;
}

int hyle_source_register_def(const hyle_source_def_t *def)
{
	size_t n;
	size_t i;
	hyle_field_t *hf;
	hyle_source_def_t *copy;
	unsigned fields_hd;

	if (!def || !def->id || !def->id[0] || !def->key_field ||
	    !def->key_field[0] || !def->fields || def->field_count == 0 ||
	    !def->items_path)
		return -1;

	if (hyle_source_find(def->id))
		return -1;

	n = def->field_count;
	hf = malloc(n * sizeof(hyle_field_t));
	if (!hf)
		return -1;

	for (i = 0; i < n; i++) {
		const hyle_source_field_t *sf = &def->fields[i];
		hf[i].name = sf->name;
		hf[i].type = sf->type;
		hf[i].writable = sf->writable;
		hf[i].target_source = sf->target_source;
		hf[i].inverse_name = sf->inverse_name;
		hf[i].required = sf->required;
		hf[i].min = sf->min;
		hf[i].max = sf->max;
		hf[i].min_length = sf->min_length;
		hf[i].max_length = sf->max_length;
		hf[i].pattern = sf->pattern;
		hf[i].searchable =
		        (sf->type == HYLE_FIELD_STRING ||
		         sf->type == HYLE_FIELD_NULLABLE_STRING ||
		         sf->type == HYLE_FIELD_DERIVED);
		hf[i].combine =
		        (sf->filter_mode && strcmp(sf->filter_mode, "and") == 0)
		                ? 1
		                : 0;
		hf[i].derive_key = sf->derive_key;
	}

	copy = malloc(sizeof(*copy));
	if (!copy) {
		free(hf);
		return -1;
	}
	*copy = *def;
	copy->id = strdup(def->id);
	copy->key_field = strdup(def->key_field);
	copy->items_path = def->items_path ? strdup(def->items_path) : NULL;
	copy->display_field[0] = '\0';
	if (copy->key_field && copy->key_field[0] && strcmp(copy->key_field, "id") != 0) {
		snprintf(copy->display_field, sizeof(copy->display_field), "%s", copy->key_field);
	} else if (copy->fields && copy->field_count > 0) {
		for (size_t fi = 0; fi < copy->field_count; fi++) {
			const hyle_source_field_t *sf = &copy->fields[fi];
			if (strcmp(sf->name, "id") == 0 || sf->type == HYLE_FIELD_INVERSE)
				continue;
			if (sf->type == HYLE_FIELD_STRING || sf->type == HYLE_FIELD_NULLABLE_STRING) {
				snprintf(copy->display_field, sizeof(copy->display_field), "%s", sf->name);
				break;
			}
		}
	}
	if (def->store.user == def->items_path)
		copy->store.user = (void *)copy->items_path;

	if (!copy->store.ops) {
		if (copy->flags & HYLE_SOURCE_FLAG_VOLATILE) {
			copy->store = hyle_source_store_mem();
		} else {
			copy->store = hyle_source_store_fs(copy->items_path);
		}
	}

	fields_hd = hyle_registry_register(
	        copy->id, hf, n, def->record_id, def->flags | CM_SORTED, copy);

	if (!fields_hd) {
		free(hf);
		free(copy);
		return -1;
	}

	copy->source_hd = hyle_registry_get_row_hd(def->id);
	copy->fields_hd = fields_hd;

	if (def->record_id > 0) {
		for (i = 0; i < n; i++) {
			const hyle_source_field_t *sf = &def->fields[i];
			hyle_source_def_t *target;

			if ((sf->type != HYLE_FIELD_REFERENCE &&
			     sf->type != HYLE_FIELD_MULTI_REFERENCE) ||
			    !sf->target_source)
				continue;
			target = hyle_source_find(sf->target_source);
			if (target && target->fields_hd)
				corm_record_field_set_target_hd(
				        def->record_id, sf->name,
				        target->fields_hd);
		}
	}

	if (copy->store.ops && copy->store.ops->scan)
		copy->store.ops->scan(&copy->store, copy);

	return 0;
}

unsigned hyle_source_query_dataset(
        const char *dataset_id,
        const char *query_str)
{
	hyle_query_t query;
	char *qs_copy = NULL;
	hyle_row_set_t output;
	size_t total;
	char tbuf[16];

	if (!dataset_id || !hyle_registry_get_user(dataset_id))
		return 0;

	memset(&query, 0, sizeof(query));

	if (query_str && query_str[0]) {
		qs_copy = strdup(query_str);
		if (!qs_copy)
			return 0;
		hyle_parse_query(qs_copy, &query);

		const hyle_source_def_t *sdef = hyle_source_find(dataset_id);
		if (sdef) {
			for (unsigned fi = 0; fi < query.filter_count; fi++) {
				hyle_field_filter_t *f = &query.filters[fi];
				for (size_t sj = 0; sj < sdef->field_count;
				     sj++)
				{
					if (strcmp(sdef->fields[sj].name,
					           f->field) == 0 &&
					    (sdef->fields[sj].type ==
					             HYLE_FIELD_MULTI_REFERENCE ||
					     sdef->fields[sj].type ==
					             HYLE_FIELD_REFERENCE))
					{
						char slug[256];
						source_util_slugify(
						        f->value,
						        strlen(f->value), slug,
						        sizeof(slug));
						f->value = strdup(slug);
						break;
					}
				}
			}
		}
	}

	memset(&output, 0, sizeof(output));
	total = 0;

	if (hyle_registry_query(dataset_id, &query, &output, &total) != 0) {
		hyle_query_clear(&query);
		free(qs_copy);
		return 0;
	}

	snprintf(tbuf, sizeof(tbuf), "%zu", total);
	corm_put(output.row_hd, "__total__", tbuf);

	hyle_query_clear(&query);
	free(qs_copy);
	return output.row_hd;
}

unsigned hyle_source_get_data_hd(const char *dataset_id)
{
	return hyle_registry_get_row_hd(dataset_id);
}

unsigned hyle_source_get_fields_hd(const char *dataset_id)
{
	return hyle_registry_get_fields_hd(dataset_id);
}

static unsigned source_build_schema_hd(const hyle_source_def_t *def)
{
	unsigned hd;
	size_t i;
	char buf[512];
	const hyle_source_field_t *f;

	hd = corm_open(NULL, NULL, CM_STR, CM_STR, 0x3FF, 0);
	if (!hd)
		return 0;

	for (i = 0; i < def->field_count; i++) {
		f = &def->fields[i];
		if (f->type == HYLE_FIELD_REFERENCE ||
		    f->type == HYLE_FIELD_MULTI_REFERENCE)
		{
			char mode_suf[20] = "";

			if (f->filter_mode &&
			    strcmp(f->filter_mode, "and") == 0)
				snprintf(
				        mode_suf, sizeof(mode_suf),
				        ",\"m\":\"and\"");
			if (f->filter_style && f->filter_style[0]) {
				snprintf(
				        buf, sizeof(buf),
				        "{\"t\":%d,\"s\":\"%s\",\"f\":\"%s\"%"
				        "s}",
				        (int)f->type,
				        f->target_source ? f->target_source
				                         : "",
				        f->filter_style, mode_suf);
			} else {
				snprintf(
				        buf, sizeof(buf),
				        "{\"t\":%d,\"s\":\"%s\"%s}",
				        (int)f->type,
				        f->target_source ? f->target_source
				                         : "",
				        mode_suf);
			}
		} else if (f->type == HYLE_FIELD_INVERSE) {
			snprintf(
			        buf, sizeof(buf),
			        "{\"t\":%d,\"s\":\"%s\",\"i\":\"%s\"}",
			        (int)f->type,
			        f->target_source ? f->target_source : "",
			        f->inverse_name ? f->inverse_name : "");
		} else {
			snprintf(buf, sizeof(buf), "{\"t\":%d}", (int)f->type);
		}
		corm_put(hd, f->name, buf);
	}

	return hd;
}

unsigned hyle_source_get_schema_hd(const char *dataset_id)
{
	hyle_source_def_t *def = hyle_source_find(dataset_id);
	if (!def)
		return 0;
	if (def->schema_hd)
		return def->schema_hd;
	def->schema_hd = source_build_schema_hd(def);
	return def->schema_hd;
}

const hyle_source_list_view_t *hyle_source_get_list_view(
        const char *dataset_id)
{
	const hyle_source_def_t *def = hyle_source_find(dataset_id);
	return def ? def->list_view : NULL;
}

int hyle_source_def_to_corm(
        const hyle_source_desc_t *defs, int count, void *out)
{
	corm_record_field_t *qf = (corm_record_field_t *)out;
	int n = 0;
	int i;
	for (i = 0; i < count; i++) {
		const hyle_source_desc_t *d = &defs[i];
		if (!d->key || d->kind >= 3)
			continue;
		qf[n].name = d->key;
		qf[n].type = (uint32_t)d->qm_type;
		if (d->is_array && qf[n].type == CM_REFERENCE)
			qf[n].type = CM_MULTI_REFERENCE;
		qf[n].offset = d->offset;
		qf[n].max_size = d->size;
		qf[n].target_record = 0;
		qf[n].target_hd = 0;
		qf[n].inverse = d->ref_inverse;
		n++;
	}
	return n;
}

int hyle_source_def_to_source_fields(
        const hyle_source_desc_t *defs, int count, void *out)
{
	hyle_source_field_t *sf = (hyle_source_field_t *)out;
	int n = 0;
	int i;
	for (i = 0; i < count; i++) {
		const hyle_source_desc_t *d = &defs[i];
		if (d->kind == HYLE_KIND_INVERSE) {
			if (!d->key || !d->ref_source || !d->ref_inverse)
				continue;
			sf[n].name = d->key;
			sf[n].file = NULL;
			sf[n].type = HYLE_FIELD_INVERSE;
			sf[n].writable = 0;
			sf[n].target_source = d->ref_source;
			sf[n].inverse_name = d->ref_inverse;
			sf[n].required = 0;
			sf[n].min = 0;
			sf[n].max = 0;
			sf[n].min_length = 0;
			sf[n].max_length = 0;
			sf[n].pattern = NULL;
			sf[n].filter_style = NULL;
			sf[n].filter_mode = NULL;
			n++;
			continue;
		}
		if (!d->key || d->kind >= 3)
			continue;
		sf[n].name = d->key;
		sf[n].file = d->file ? d->file : d->key;
		if (d->type == HYLE_FIELD_DERIVED)
			sf[n].file = NULL;
		sf[n].type = d->type;
		if (d->is_array && sf[n].type == HYLE_FIELD_REFERENCE)
			sf[n].type = HYLE_FIELD_MULTI_REFERENCE;
		sf[n].writable = d->writable;
		sf[n].target_source = d->ref_source;
		sf[n].inverse_name = d->ref_inverse;
		sf[n].required = d->required;
		sf[n].min = 0;
		sf[n].max = 0;
		sf[n].min_length = d->min_length;
		sf[n].max_length = 0;
		sf[n].pattern = NULL;
		sf[n].filter_style = d->filter_style;
		sf[n].filter_mode = d->filter_mode;
		sf[n].derive_key = d->derive_key;
		n++;
	}
	return n;
}

int hyle_source_build_state_specs(
        const hyle_source_desc_t *fields,
        hyle_source_state_field_t *specs,
        int max_specs)
{
	int i = 0;
	for (const hyle_source_desc_t *f = fields; f->key && i < max_specs - 1;
	     f++)
	{
		if (f->kind == HYLE_SF_EXCLUDE || f->kind == HYLE_SF_REF_DISPLAY) {
			specs[i].name = f->key;
			specs[i].kind = (hyle_source_state_kind_t)f->kind;
			i++;
		}
	}
	specs[i].name = NULL;
	specs[i].kind = 0;
	return i;
}

static void patch_corm_targets(
        corm_record_field_t *qf, int n, const hyle_source_desc_t *defs, int count)
{
	int i;
	for (i = 0; i < n && i < count; i++) {
		if (defs[i].ref_source && qf[i].target_record == 0) {
			hyle_source_def_t *src = hyle_source_find(defs[i].ref_source);
			if (src)
				qf[i].target_record = src->record_id;
		}
	}
}

size_t hyle_source_inv_keys(
        const char *dataset_id,
        const char *field,
        uint32_t target_pos,
        const char **keys,
        size_t max)
{
	uint32_t buf[4096];
	unsigned fhd;
	size_t n, i, count;

	if (!dataset_id || !field || !keys || max == 0)
		return 0;

	fhd = hyle_source_get_fields_hd(dataset_id);
	if (!fhd)
		return 0;

	n = corm_inv_get(fhd, field, target_pos, buf, 4096);
	count = n < max ? n : max;

	for (i = 0; i < count; i++)
		keys[i] = corm_get_key(fhd, buf[i]);

	return count;
}

const char *hyle_source_inv_key_at(
        const char *dataset_id,
        const char *field,
        uint32_t target_pos,
        size_t index)
{
	uint32_t buf[4096];
	unsigned fhd;
	size_t n;

	if (!dataset_id || !field)
		return NULL;

	fhd = hyle_source_get_fields_hd(dataset_id);
	if (!fhd)
		return NULL;

	n = corm_inv_get(fhd, field, target_pos, buf, 4096);
	if (index >= n)
		return NULL;
	return corm_get_key(fhd, buf[index]);
}

size_t hyle_source_find_referencing(
        const char *source_dataset,
        const char *ref_field,
        const char *target_id,
        const char **ids_out,
        size_t max)
{
	if (!source_dataset || !ref_field || !target_id || !ids_out || max == 0)
		return 0;

	hyle_source_def_t *src_def = hyle_source_find(source_dataset);
	if (!src_def || !src_def->fields_hd)
		return 0;

	uint32_t target_pos = CM_MISS;
	const char *target_dataset = NULL;
	if (src_def->fields) {
		for (size_t i = 0; i < src_def->field_count; i++) {
			if (src_def->fields[i].name &&
			    strcmp(src_def->fields[i].name, ref_field) == 0)
			{
				target_dataset = src_def->fields[i].target_source;
				break;
			}
		}
	}
	if (!target_dataset && src_def->defs) {
		for (int i = 0; i < src_def->def_count; i++) {
			if (src_def->defs[i].key &&
			    strcmp(src_def->defs[i].key, ref_field) == 0)
			{
				target_dataset = src_def->defs[i].ref_source;
				break;
			}
		}
	}

	if (target_dataset) {
		unsigned tgt_fhd = hyle_source_get_fields_hd(target_dataset);
		if (tgt_fhd)
			target_pos = corm_pos(tgt_fhd, target_id);
	}

	if (target_pos == CM_MISS)
		target_pos = corm_pos(src_def->fields_hd, target_id);

	if (target_pos == CM_MISS)
		return 0;

	return hyle_source_inv_keys(source_dataset, ref_field, target_pos, ids_out, max);
}

size_t hyle_source_for_each_referencing(
        const char *source_dataset,
        const char *ref_field,
        const char *target_id,
        hyle_source_ref_cb_t cb,
        void *user)
{
	const char *keys[256];
	size_t n = hyle_source_find_referencing(
	        source_dataset, ref_field, target_id, keys, 256);
	if (cb) {
		for (size_t i = 0; i < n; i++)
			cb(keys[i], user);
	}
	return n;
}

const char *hyle_corm_get_field_str(
        unsigned hd,
        const char *id,
        const char *field)
{
	static __thread char key[512];
	snprintf(key, sizeof(key), "%s:%s", id, field);
	return corm_get(hd, key);
}

uint32_t hyle_source_setup(
        const char *source_id,
        const char *key_field,
        size_t record_size,
        const char *items_path,
        const hyle_source_desc_t *defs,
        int field_count,
        unsigned flags,
        const hyle_source_list_view_t *list_view)
{
	char record_name[256];
	const char *p;
	char *q, *kf;
	corm_record_field_t qf[(size_t)field_count];
	int n_qf, n_sf;
	uint32_t record_id;
	hyle_source_field_t *sf;

	snprintf(record_name, sizeof(record_name), "%s", source_id);
	for (p = source_id, q = record_name; *p; p++, q++) {
		if (*p == '.')
			*q = '_';
		else
			*q = *p;
	}
	*q = '\0';

	kf = (char *)(key_field ? key_field : "id");

	n_qf = hyle_source_def_to_corm(defs, field_count, qf);
	patch_corm_targets(qf, n_qf, defs, field_count);
	record_id = corm_record_register(
	        record_name, record_size, qf, (size_t)n_qf);

	sf = calloc((size_t)field_count, sizeof(hyle_source_field_t));
	if (!sf)
		return 0;
	n_sf = hyle_source_def_to_source_fields(defs, field_count, sf);

	hyle_source_store_t init_store;
	if (flags & HYLE_SOURCE_FLAG_VOLATILE) {
		init_store = hyle_source_store_mem();
	} else {
		init_store = hyle_source_store_fs(items_path);
	}

	if (hyle_source_register_def(&(hyle_source_def_t){
	            .id = source_id,
	            .key_field = kf,
	            .items_path = items_path,
	            .access_policy = HYLE_SOURCE_ACCESS_PUBLIC,
	            .fields = sf,
	            .field_count = (size_t)n_sf,
	            .defs = defs,
	            .def_count = field_count,
	            .record_size = record_size,
	            .record_id = record_id,
	            .flags = flags,
	            .list_view = list_view,
	            .store = init_store,
	    }) != 0)
	{
		free(sf);
		return 0;
	}

	return record_id;
}

const hyle_source_desc_t *hyle_source_get_desc(
        const char *dataset_id,
        int *count_out)
{
	const hyle_source_def_t *def = hyle_source_find(dataset_id);
	if (!def) {
		if (count_out)
			*count_out = 0;
		return NULL;
	}
	if (count_out)
		*count_out = def->def_count;
	return def->defs;
}

unsigned hyle_source_parse_row_data_custom(
        const hyle_source_def_t *def,
        hyle_field_getter_fn get_single,
        hyle_multi_field_getter_fn get_multi,
        void *user)
{
	unsigned hd;
	size_t i;

	if (!def)
		return 0;

	hd = corm_open(NULL, "row_data", CM_STR, CM_STR, 0x1F, 0);
	if (hd == 0)
		return 0;

	for (i = 0; i < def->field_count; i++) {
		const hyle_source_field_t *f = &def->fields[i];
		int fld_len;
		char *val;

		if (!f->writable)
			continue;

		if (f->type == HYLE_FIELD_MULTI_REFERENCE && get_multi) {
			int all_len = get_multi(f->name, NULL, 0, user);

			if (all_len > 0) {
				val = malloc((size_t)all_len + 1);
				if (!val) {
					corm_close(hd);
					return 0;
				}
				if (get_multi(f->name, val, (size_t)all_len + 1, user) != all_len) {
					free(val);
					corm_close(hd);
					return 0;
				}
				corm_put(hd, f->name, val);
				free(val);
				continue;
			}
			if (all_len == 0)
				continue;
		}

		if (!get_single)
			continue;

		fld_len = get_single(f->name, NULL, 0, user);
		if (fld_len <= 0)
			continue;

		val = malloc((size_t)fld_len + 1);
		if (!val) {
			corm_close(hd);
			return 0;
		}
		if (get_single(f->name, val, (size_t)fld_len + 1, user) != fld_len) {
			free(val);
			corm_close(hd);
			return 0;
		}
		corm_put(hd, f->name, val);
		free(val);
	}
	return hd;
}

size_t hyle_source_get_record_size(
        const char *dataset_id)
{
	const hyle_source_def_t *def = hyle_source_find(dataset_id);
	return def ? def->record_size : 0;
}
