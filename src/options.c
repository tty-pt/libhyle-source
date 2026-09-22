#include "hyle-source/hyle_source.h"
#include "source_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ttypt/qmap.h>
#include <hyle/registry.h>

int hyle_source_get_display_field(const char *dataset_id, char *out, size_t sz)
{
	unsigned schema_hd;
	uint32_t cur;
	const void *key;
	const void *val;
	hyle_source_def_t *def;

	if (!out || sz == 0)
		return -1;
	out[0] = '\0';
	if (!dataset_id || !dataset_id[0])
		return -1;

	def = hyle_source_find(dataset_id);
	if (def && def->display_field[0]) {
		snprintf(out, sz, "%s", def->display_field);
		return 0;
	}

	if (def && def->key_field && def->key_field[0] && strcmp(def->key_field, "id") != 0) {
		snprintf(out, sz, "%s", def->key_field);
		return 0;
	}

	if (def && def->fields && def->field_count > 0) {
		for (size_t i = 0; i < def->field_count; i++) {
			const hyle_source_field_t *f = &def->fields[i];
			if (strcmp(f->name, "id") == 0 || f->type == HYLE_FIELD_INVERSE)
				continue;
			if (f->type == HYLE_FIELD_STRING || f->type == HYLE_FIELD_NULLABLE_STRING) {
				snprintf(out, sz, "%s", f->name);
				return 0;
			}
		}
	}

	schema_hd = hyle_source_get_schema_hd(dataset_id);
	if (!schema_hd)
		return -1;

	cur = qmap_iter(schema_hd, NULL, 0);
	while (qmap_next(&key, &val, cur)) {
		if (strcmp((const char *)key, "id") == 0)
			continue;
		if (val && strstr((const char *)val, "\"i\":"))
			continue;
		snprintf(out, sz, "%s", (const char *)key);
		break;
	}
	qmap_fin(cur);
	return out[0] ? 0 : -1;
}

const char *hyle_source_get_item_label(
        const char *dataset_id, const char *row_id, const char *display_field,
        char *out, size_t sz)
{
	unsigned fields_hd;
	const char *name = NULL;
	char df_buf[64] = { 0 };

	if (!row_id)
		return "";

	fields_hd = hyle_source_get_fields_hd(dataset_id);
	if (!fields_hd) {
		if (out && sz)
			snprintf(out, sz, "%s", row_id);
		return row_id;
	}

	if (!display_field || !display_field[0]) {
		hyle_source_def_t *def = hyle_source_find(dataset_id);
		if (def && def->display_field[0]) {
			display_field = def->display_field;
		} else {
			hyle_source_get_display_field(dataset_id, df_buf, sizeof(df_buf));
			display_field = df_buf;
		}
	}

	if (display_field && display_field[0]) {
		char name_key[320];
		snprintf(name_key, sizeof(name_key), "%s:%s", row_id, display_field);
		name = (const char *)qmap_get(fields_hd, name_key);
	}

	const char *res = name ? name : row_id;
	if (out && sz)
		snprintf(out, sz, "%s", res);
	return res;
}

int hyle_source_resolve_options(
        const char *dataset_id, const char *q, int page0, int per_page,
        hyle_option_t *opts, int max, int *total_out,
        char (*id_buf)[64], char (*label_buf)[256])
{
	char qs[1024];
	char display_field[64];
	unsigned result_hd;
	const char *total_str;
	uint32_t cur;
	const void *rkey;
	const void *rval;
	int n = 0;

	if (total_out)
		*total_out = 0;
	if (!dataset_id || !dataset_id[0] || max <= 0 || !opts || !id_buf || !label_buf)
		return 0;

	if (q && q[0]) {
		snprintf(
		        qs, sizeof(qs), "q=%s&page=%d&per_page=%d",
		        q, page0 + 1, per_page > 0 ? per_page : 15);
	} else {
		snprintf(
		        qs, sizeof(qs), "page=%d&per_page=%d",
		        page0 + 1, per_page > 0 ? per_page : 15);
	}

	result_hd = hyle_source_query_dataset(dataset_id, qs);
	if (!result_hd)
		return 0;

	total_str = (const char *)qmap_get(result_hd, "__total__");
	if (total_out)
		*total_out = total_str ? atoi(total_str) : 0;

	hyle_source_get_display_field(dataset_id, display_field, sizeof(display_field));

	cur = qmap_iter(result_hd, NULL, 0);
	while (n < max && qmap_next(&rkey, &rval, cur)) {
		const char *row_id;
		const char *name;

		if (strcmp((const char *)rkey, "__total__") == 0)
			continue;
		row_id = (const char *)rkey;
		snprintf(id_buf[n], sizeof(id_buf[n]), "%s", row_id);
		name = hyle_source_get_item_label(
		        dataset_id, row_id, display_field, label_buf[n], sizeof(label_buf[n]));
		opts[n].id = id_buf[n];
		opts[n].label = name;
		n++;
	}
	qmap_fin(cur);
	qmap_close(result_hd);
	return n;
}

int hyle_source_resolve_tokens(
        const char *dataset_id, const char *comma_slugs, hyle_option_t *out,
        int max, char (*id_buf)[64], char (*label_buf)[256])
{
	char display_field[64];
	unsigned fields_hd;
	const char *p = comma_slugs;
	int n = 0;

	if (!dataset_id || !dataset_id[0] || !p || !p[0] || max <= 0 || !out || !id_buf || !label_buf)
		return 0;

	fields_hd = hyle_source_get_fields_hd(dataset_id);
	if (!fields_hd)
		return 0;

	hyle_source_get_display_field(dataset_id, display_field, sizeof(display_field));

	while (*p && n < max) {
		const char *comma = strpbrk(p, ",\n\r");
		size_t len = comma ? (size_t)(comma - p) : strlen(p);
		char token[128];
		char slug_buf[128] = { 0 };
		const char *slug = NULL;

		if (len >= sizeof(token))
			len = sizeof(token) - 1;
		memcpy(token, p, len);
		token[len] = '\0';
		char *ttrim = token;
		while (*ttrim == ' ' || *ttrim == '\t')
			ttrim++;
		size_t tlen = strlen(ttrim);
		while (tlen > 0 &&
		       (ttrim[tlen - 1] == ' ' || ttrim[tlen - 1] == '\t' ||
		        ttrim[tlen - 1] == '\r'))
			ttrim[--tlen] = '\0';

		if (ttrim[0]) {
			if (strspn(ttrim, "0123456789") == strlen(ttrim)) {
				slug = qmap_get_key(
				        fields_hd, (uint32_t)atoi(ttrim));
				if (slug && !slug[0])
					slug = NULL;
			}
			if (!slug) {
				source_util_slugify(
				        ttrim, strlen(ttrim), slug_buf,
				        sizeof(slug_buf));
				if (slug_buf[0] &&
				    qmap_pos(fields_hd, slug_buf) != UINT32_MAX)
					slug = qmap_get_key(
					        fields_hd,
					        qmap_pos(fields_hd, slug_buf));
			}
			if (!slug) {
				if (qmap_pos(fields_hd, ttrim) != UINT32_MAX)
					slug = qmap_get_key(
					        fields_hd,
					        qmap_pos(fields_hd, ttrim));
			}
			if (!slug) {
				if (!slug_buf[0])
					source_util_slugify(
					        ttrim, strlen(ttrim), slug_buf,
					        sizeof(slug_buf));
				if (slug_buf[0])
					slug = slug_buf;
			}
			if (!slug)
				slug = ttrim;

			snprintf(id_buf[n], sizeof(id_buf[n]), "%.60s", slug);
			hyle_source_get_item_label(
			        dataset_id, id_buf[n], display_field, label_buf[n], sizeof(label_buf[n]));
			out[n].id = id_buf[n];
			out[n].label = label_buf[n];
			n++;
		}
		if (!comma)
			break;
		while (*comma && (*comma == ',' || *comma == '\n' ||
		                  *comma == '\r' || *comma == ' '))
			comma++;
		p = comma;
	}
	return n;
}

int hyle_source_normalize_tokens_to_slugs(
        const char *dataset_id, const char *raw, char *out, size_t out_sz)
{
	char display_field[64];
	unsigned fields_hd;
	const char *p = raw;
	size_t off = 0;

	if (!out || out_sz == 0)
		return -1;
	out[0] = '\0';
	if (!dataset_id || !raw || !raw[0])
		return 0;

	fields_hd = hyle_source_get_fields_hd(dataset_id);
	if (!fields_hd)
		return -1;

	hyle_source_get_display_field(dataset_id, display_field, sizeof(display_field));

	while (*p && off + 2 < out_sz) {
		const char *end = strpbrk(p, "\r\n,");
		size_t len = end ? (size_t)(end - p) : strlen(p);
		char token[128];
		const char *slug = NULL;

		if (len >= sizeof(token))
			len = sizeof(token) - 1;
		memcpy(token, p, len);
		token[len] = '\0';
		char *ttrim = token;
		while (*ttrim == ' ' || *ttrim == '\t')
			ttrim++;
		size_t tlen = strlen(ttrim);
		while (tlen > 0 &&
		       (ttrim[tlen - 1] == ' ' || ttrim[tlen - 1] == '\t' ||
		        ttrim[tlen - 1] == '\r'))
			ttrim[--tlen] = '\0';

		if (ttrim[0]) {
			if (strspn(ttrim, "0123456789") == strlen(ttrim)) {
				slug = qmap_get_key(
				        fields_hd, (uint32_t)atoi(ttrim));
				if (slug && !slug[0])
					slug = NULL;
			}
			/* Fallback reverse-lookup by display name */
			if (!slug && display_field[0]) {
				char slug_buf[128];
				source_util_slugify(
				        ttrim, strlen(ttrim), slug_buf,
				        sizeof(slug_buf));
				if (slug_buf[0] &&
				    qmap_pos(fields_hd, slug_buf) != UINT32_MAX)
				{
					slug = qmap_get_key(
					        fields_hd,
					        qmap_pos(fields_hd, slug_buf));
				} else {
					unsigned result_hd;
					char qs[512];
					snprintf(
					        qs, sizeof(qs), "%s=%s",
					        display_field, ttrim);
					result_hd = hyle_source_query_dataset(dataset_id, qs);
					if (result_hd) {
						uint32_t cur = qmap_iter(result_hd, NULL, 0);
						const void *rk, *rv;
						while (qmap_next(&rk, &rv, cur)) {
							if (strcmp((const char *)rk, "__total__") != 0) {
								slug = (const char *)rk;
								break;
							}
						}
						qmap_fin(cur);
						qmap_close(result_hd);
					}
				}
			}
			if (!slug)
				slug = ttrim;
			if (off > 0)
				out[off++] = ',';
			snprintf(out + off, out_sz - off, "%.60s", slug);
			off = strlen(out);
		}
		if (!end)
			break;
		p = end + 1;
	}
	return 0;
}

int hyle_source_get_enum_options(
        const char *dataset_id, hyle_option_t *pool, int pool_avail,
        char (*id_buf)[64], char (*label_buf)[256])
{
	unsigned row_hd;
	char display_field[64] = "";
	int nopts = 0;
	uint32_t cur;
	const void *key;
	const void *val;

	if (!dataset_id || !dataset_id[0] || !pool || pool_avail <= 0 || !id_buf || !label_buf)
		return 0;

	row_hd = hyle_source_get_data_hd(dataset_id);
	if (!row_hd)
		return 0;

	hyle_source_get_display_field(dataset_id, display_field, sizeof(display_field));

	cur = qmap_iter(row_hd, NULL, 0);
	while (qmap_next(&key, &val, cur) && nopts < pool_avail) {
		const char *row_id = (const char *)key;
		snprintf(id_buf[nopts], sizeof(id_buf[nopts]), "%s", row_id);
		const char *name = hyle_source_get_item_label(
		        dataset_id, row_id, display_field, label_buf[nopts], sizeof(label_buf[nopts]));
		pool[nopts].id = id_buf[nopts];
		pool[nopts].label = name;
		nopts++;
	}
	qmap_fin(cur);

	return nopts;
}

static void extract_datalist_id(const char *in, char *out, size_t sz)
{
	const char *lb = strrchr(in, '[');
	const char *rb = strrchr(in, ']');
	if (!lb || !rb || rb <= lb + 1) {
		lb = strrchr(in, '(');
		rb = strrchr(in, ')');
	}
	if (lb && rb && rb > lb + 1) {
		size_t len = rb - lb - 1;
		if (len < sz) {
			memcpy(out, lb + 1, len);
			out[len] = '\0';
			return;
		}
	}
	snprintf(out, sz, "%s", in);
}

int hyle_source_ordered_sync_form_custom(
	const char *source_id,
	const char *partition_id,
	const char *amount_param,
	const char *remove_param_prefix,
	const hyle_ordered_field_sync_t *fields,
	size_t n_fields,
	hyle_field_getter_fn get_field,
	void *user)
{
	char amt_buf[16] = { 0 };
	int amount = 0;

	if (!source_id || !partition_id || !fields || n_fields == 0 || !get_field)
		return -1;

	if (get_field(amount_param ? amount_param : "amount", amt_buf, sizeof(amt_buf), user) > 0)
		amount = atoi(amt_buf);

	hyle_ordered_clear(source_id, partition_id);

	for (int i = 0; i < amount; i++) {
		char rem_name[64];
		char rem_val[16];
		snprintf(rem_name, sizeof(rem_name), "%s_%d",
			remove_param_prefix ? remove_param_prefix : "remove", i);
		if (get_field(rem_name, rem_val, sizeof(rem_val), user) > 0 && rem_val[0] && strcmp(rem_val, "0") != 0)
			continue;

		char row_vals[16][128];
		const char *names[16];
		const char *vals[16];
		int valid = 1;

		for (size_t f = 0; f < n_fields && f < 16; f++) {
			char fld_name[64];
			snprintf(fld_name, sizeof(fld_name), "%s_%d", fields[f].form_field_prefix, i);
			char raw_val[128] = { 0 };
			if (get_field(fld_name, raw_val, sizeof(raw_val), user) > 0) {
				if (fields[f].is_primary_key) {
					extract_datalist_id(raw_val, row_vals[f], sizeof(row_vals[f]));
					if (!row_vals[f][0]) {
						valid = 0;
						break;
					}
					/* Auto-resolve partition references if any */
					unsigned song_fhd = hyle_source_get_fields_hd("song.items");
					unsigned repo_fhd = hyle_source_get_fields_hd("grp.songs");
					if (song_fhd && repo_fhd && qmap_pos(song_fhd, row_vals[f]) == QM_MISS) {
						uint32_t rp = qmap_pos(repo_fhd, row_vals[f]);
						if (rp != QM_MISS) {
							const char *rs = qmap_field_get(repo_fhd, row_vals[f], "song");
							if (rs && rs[0])
								snprintf(row_vals[f], sizeof(row_vals[f]), "%s", rs);
						}
					}
				} else {
					snprintf(row_vals[f], sizeof(row_vals[f]), "%s", raw_val);
				}
			} else {
				if (fields[f].is_primary_key) {
					valid = 0;
					break;
				}
				snprintf(row_vals[f], sizeof(row_vals[f]), "%s",
					fields[f].default_value ? fields[f].default_value : "");
			}
			names[f] = fields[f].schema_field_name;
			vals[f] = row_vals[f];
		}

		if (valid) {
			hyle_ordered_append(source_id, partition_id, names, vals, n_fields);
		}
	}
	hyle_ordered_save(source_id, partition_id);
	return 0;
}

const char *hyle_source_ordered_get_field(
        const char *source_id, const char *partition_val, int index,
        const char *field)
{
	if (!source_id || !partition_val || index < 0 || !field)
		return NULL;
	const char *key =
	        hyle_ordered_key_at(source_id, partition_val, index);
	if (!key)
		return NULL;
	unsigned fhd = hyle_source_get_fields_hd(source_id);
	if (!fhd)
		return NULL;
	return hyle_qmap_get_field_str(fhd, key, field);
}

int hyle_source_ordered_set_field(
        const char *source_id, const char *partition_val, int index,
        const char *field, const char *value)
{
	if (!source_id || !partition_val || index < 0 || !field)
		return -1;
	const char *key =
	        hyle_ordered_key_at(source_id, partition_val, index);
	if (!key)
		return -1;
	const char *names[1] = { field };
	const char *vals[1] = { value ? value : "" };
	hyle_registry_put(source_id, key, names, vals, 1);
	hyle_ordered_save(source_id, partition_val);
	return 0;
}

int hyle_source_ordered_remove_and_save(
        const char *source_id, const char *partition_val, int index)
{
	if (!source_id || !partition_val || index < 0)
		return -1;
	int count = hyle_ordered_count(source_id, partition_val);
	if (index >= count)
		return -1;
	hyle_ordered_remove_at(source_id, partition_val, index);
	hyle_ordered_save(source_id, partition_val);
	return 0;
}

int hyle_source_ordered_append_and_save(
        const char *source_id, const char *partition_val, const char **names,
        const char **vals, size_t count)
{
	if (!source_id || !partition_val || !names || !vals || count == 0)
		return -1;
	int rc = hyle_ordered_append(
	        source_id, partition_val, names, vals, count);
	if (rc == 0)
		hyle_ordered_save(source_id, partition_val);
	return rc;
}

int hyle_source_ordered_for_each(
        const char *source_id, const char *partition_val,
        hyle_source_ordered_each_fn fn, void *user)
{
	if (!source_id || !partition_val || !fn)
		return 0;
	int total = hyle_ordered_count(source_id, partition_val);
	unsigned fhd = hyle_source_get_fields_hd(source_id);
	for (int i = 0; i < total; i++) {
		const char *key =
		        hyle_ordered_key_at(source_id, partition_val, i);
		if (key)
			fn(i, key, fhd, user);
	}
	return total;
}

int hyle_source_ordered_find(
        const char *source_id, const char *partition_val, const char *field,
        const char *val)
{
	if (!source_id || !partition_val || !field || !val)
		return -1;
	int total = hyle_ordered_count(source_id, partition_val);
	for (int i = 0; i < total; i++) {
		const char *fval =
		        hyle_source_ordered_get_field(source_id, partition_val, i, field);
		if (fval && strcmp(fval, val) == 0)
			return i;
	}
	return -1;
}

int hyle_source_ordered_remove_matching(
        const char *source_id, const char *partition_val, const char *field,
        const char *val)
{
	int idx = hyle_source_ordered_find(source_id, partition_val, field, val);
	if (idx < 0)
		return -1;
	return hyle_source_ordered_remove_and_save(source_id, partition_val, idx);
}

int hyle_source_ordered_replace_row(
        const char *source_id, const char *partition_val, int index,
        const char **names, const char **vals, size_t count)
{
	if (!source_id || !partition_val || index < 0 || !names || !vals || count == 0)
		return -1;
	const char *key =
	        hyle_ordered_key_at(source_id, partition_val, index);
	if (!key)
		return -1;
	hyle_registry_put(source_id, key, names, vals, count);
	hyle_ordered_save(source_id, partition_val);
	return 0;
}

const char *hyle_source_get_field(
        const char *source_id, const char *item_id, const char *field)
{
	if (!source_id || !item_id || !field)
		return NULL;
	unsigned fhd = hyle_source_get_fields_hd(source_id);
	if (!fhd)
		return NULL;
	return hyle_qmap_get_field_str(fhd, item_id, field);
}

int hyle_source_set_field(
        int fd, const char *source_id, const char *item_id, const char *field,
        const char *value)
{
	if (!source_id || !item_id || !field)
		return -1;
	unsigned dh = qmap_open(NULL, "row_data", QM_STR, QM_STR, 0x1F, 0);
	if (!dh)
		return -1;
	qmap_put(dh, field, value ? value : "");
	int rc = hyle_source_update_item(fd, source_id, item_id, dh);
	qmap_close(dh);

	unsigned fhd = hyle_source_get_fields_hd(source_id);
	if (fhd)
		qmap_field_put(fhd, item_id, field, value ? value : "");

	return rc == 0 ? 0 : (fhd ? 0 : rc);
}

int hyle_source_get_field_int(
        const char *source_id, const char *item_id, const char *field,
        int def_val)
{
	const char *str = hyle_source_get_field(source_id, item_id, field);
	if (!str || !str[0])
		return def_val;
	char *end = NULL;
	long v = strtol(str, &end, 10);
	if (end == str)
		return def_val;
	return (int)v;
}

int hyle_source_set_field_int(
        int fd, const char *source_id, const char *item_id, const char *field,
        int val)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%d", val);
	return hyle_source_set_field(fd, source_id, item_id, field, buf);
}
