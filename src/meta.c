#include "hyle-source/hyle_source.h"
#include "source_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef struct {
	const char *name;
	char *buf;
	size_t sz;
} meta_field_internal_t;

int hyle_source_def_to_meta_fields(
        const hyle_source_desc_t *defs, int count,
        const void *record, void *out)
{
	meta_field_internal_t *mf = (meta_field_internal_t *)out;
	int n = 0;
	int i;
	for (i = 0; i < count; i++) {
		const hyle_source_desc_t *d = &defs[i];
		if (!d->key || d->kind >= 3 || !d->in_meta)
			continue;
		mf[n].name = d->key;
		mf[n].buf = (char *)record + d->offset;
		mf[n].sz = d->size;
		n++;
	}
	return n;
}

static int internal_read_meta_file(
        const char *item_path, const char *name, char *buf, size_t sz)
{
	char p[PATH_MAX];
	FILE *mfp;

	snprintf(p, sizeof(p), "%s/%s", item_path, name);
	mfp = fopen(p, "r");
	if (!mfp)
		return -1;
	size_t n = fread(buf, 1, sz - 1, mfp);
	buf[n] = '\0';
	while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
		buf[--n] = '\0';
	fclose(mfp);
	return 0;
}

static int internal_write_meta_file(
        const char *item_path, const char *name, const char *buf, size_t sz)
{
	char p[PATH_MAX];
	snprintf(p, sizeof(p), "%s/%s", item_path, name);
	return source_util_write_file(p, buf, sz);
}

int hyle_source_meta_read(
        const char *path,
        const hyle_source_desc_t *fields,
        int count,
        void *record,
        size_t record_size)
{
	if (!path || !fields || !record || count <= 0)
		return -1;

	meta_field_internal_t f[(size_t)count];
	int n = hyle_source_def_to_meta_fields(fields, count, record, f);
	memset(record, 0, record_size);

	for (int i = 0; i < n; i++) {
		if (!f[i].name || !f[i].buf || f[i].sz == 0)
			continue;
		f[i].buf[0] = '\0';
		internal_read_meta_file(path, f[i].name, f[i].buf, f[i].sz);
	}
	return 0;
}

int hyle_source_meta_write(
        const char *path,
        const hyle_source_desc_t *fields,
        int count,
        const void *record)
{
	if (!path || !fields || !record || count <= 0)
		return -1;

	meta_field_internal_t f[(size_t)count];
	int n = hyle_source_def_to_meta_fields(fields, count, record, f);

	for (int i = 0; i < n; i++) {
		if (!f[i].name || !f[i].buf)
			continue;
		if (internal_write_meta_file(
		            path, f[i].name, f[i].buf, strlen(f[i].buf)) != 0)
			return -1;
	}
	return 0;
}
