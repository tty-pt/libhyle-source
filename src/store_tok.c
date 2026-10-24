#include "hyle-source/store.h"
#include "hyle-source/hyle_source.h"
#include "source_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <ttypt/corm.h>
#include <ttypt/lemma.h>
#include <hyle/hyle.h>
#include <hyle/registry.h>

#define HTOK_MAGIC 0x48544F4B /* "HTOK" */

static int tok_load(hyle_source_store_t *store, const hyle_source_def_t *def,
                    const char *id, unsigned *row_out)
{
	if (!store || !def || !id)
		return -1;
	if (!source_util_is_safe_id(id))
		return -1;

	const char *items_path = def->items_path ? def->items_path : (const char *)store->user;
	if (!items_path || !items_path[0])
		return -1;

	char doc_root[256] = { 0 };
	const char *root = source_util_resolve_doc_root(doc_root, sizeof(doc_root));
	char container_path[PATH_MAX];
	snprintf(container_path, sizeof(container_path), "%s/%s/%s/record.tok", root, items_path, id);

	FILE *fp = fopen(container_path, "rb");
	if (!fp) {
		/* Fallback to plaintext fs_load if record.tok not found */
		return hyle_source_store_fs(items_path).ops->load(store, def, id, row_out);
	}

	uint32_t magic = 0;
	uint16_t version = 0;
	uint16_t field_count = 0;

	if (fread(&magic, 4, 1, fp) != 1 || magic != HTOK_MAGIC) {
		fclose(fp);
		return -1;
	}
	if (fread(&version, 2, 1, fp) != 1 || fread(&field_count, 2, 1, fp) != 1) {
		fclose(fp);
		return -1;
	}

	const char *names[64];
	const char *values[64];
	char *decoded_bufs[64];
	size_t k = 0;
	size_t nb = 0;

	names[k] = "id";
	values[k] = id;
	k++;

	for (uint16_t f = 0; f < field_count && k < 64; f++) {
		uint8_t nlen = 0;
		if (fread(&nlen, 1, 1, fp) != 1)
			break;
		char fname[64];
		if (fread(fname, 1, nlen, fp) != nlen)
			break;
		fname[nlen] = '\0';

		uint16_t ntok = 0;
		if (fread(&ntok, 2, 1, fp) != 1)
			break;

		uint16_t tokens[2048];
		if (ntok > 2048 || fread(tokens, 2, ntok, fp) != ntok)
			break;

		char *dec = malloc(8192);
		if (!dec)
			break;

		size_t bw = 0;
		if (lemma_tok_decode(NULL, tokens, ntok, dec, 8192, &bw) == 0) {
			names[k] = strdup(fname);
			values[k] = dec;
			decoded_bufs[nb++] = (char *)names[k];
			decoded_bufs[nb++] = dec;
			k++;
		} else {
			free(dec);
		}
	}
	fclose(fp);

	int rc = hyle_registry_put(def->id, id, names, values, k);
	for (size_t b = 0; b < nb; b++)
		free(decoded_bufs[b]);
	if (row_out)
		*row_out = 0;
	return rc;
}

static int tok_scan(hyle_source_store_t *store, const hyle_source_def_t *def)
{
	const char *items_path = def->items_path ? def->items_path : (const char *)store->user;
	if (!items_path || !items_path[0])
		return 0;

	char doc_root[256] = { 0 };
	const char *root = source_util_resolve_doc_root(doc_root, sizeof(doc_root));
	char full[PATH_MAX];
	snprintf(full, sizeof(full), "%s/%s", root, items_path);
	DIR *dir = opendir(full);
	if (!dir)
		return 0;

	struct dirent *entry;
	while ((entry = readdir(dir))) {
		if (entry->d_name[0] == '.')
			continue;
		unsigned row_unused = 0;
		tok_load(store, def, entry->d_name, &row_unused);
	}
	closedir(dir);
	return 0;
}

static int tok_put(hyle_source_store_t *store, const hyle_source_def_t *def,
                   const char *id, unsigned data_handle)
{
	if (!store || !def || !id)
		return -1;
	if (!source_util_is_safe_id(id))
		return -1;

	const char *items_path = def->items_path ? def->items_path : (const char *)store->user;
	if (!items_path || !items_path[0])
		return -1;

	char doc_root[256] = { 0 };
	const char *root = source_util_resolve_doc_root(doc_root, sizeof(doc_root));
	char item_path[PATH_MAX];
	snprintf(item_path, sizeof(item_path), "%s/%s/%s", root, items_path, id);
	mkdir(item_path, 0755);

	char container_path[PATH_MAX + 64];
	snprintf(container_path, sizeof(container_path), "%s/record.tok", item_path);

	FILE *fp = fopen(container_path, "wb");
	if (!fp)
		return -1;

	uint32_t magic = HTOK_MAGIC;
	uint16_t version = 1;
	uint16_t field_count = 0;

	for (size_t i = 0; i < def->field_count; i++) {
		const hyle_source_field_t *f = &def->fields[i];
		if (strcmp(f->name, "id") == 0 || strcmp(f->name, "owner") == 0)
			continue;
		if (corm_get(data_handle, f->name))
			field_count++;
	}

	fwrite(&magic, 4, 1, fp);
	fwrite(&version, 2, 1, fp);
	fwrite(&field_count, 2, 1, fp);

	for (size_t i = 0; i < def->field_count; i++) {
		const hyle_source_field_t *f = &def->fields[i];
		if (strcmp(f->name, "id") == 0 || strcmp(f->name, "owner") == 0)
			continue;
		const char *val = corm_get(data_handle, f->name);
		if (!val)
			continue;

		uint8_t nlen = (uint8_t)strlen(f->name);
		fwrite(&nlen, 1, 1, fp);
		fwrite(f->name, 1, nlen, fp);

		uint16_t tokens[2048];
		size_t ntok = 0;
		if (lemma_tok_encode(NULL, val, strlen(val), tokens, 2048, &ntok) == 0) {
			uint16_t c = (uint16_t)ntok;
			fwrite(&c, 2, 1, fp);
			fwrite(tokens, 2, ntok, fp);
		} else {
			uint16_t c = 0;
			fwrite(&c, 2, 1, fp);
		}
	}
	fclose(fp);

	return 0;
}

static int tok_put_field(hyle_source_store_t *store, const hyle_source_def_t *def,
                         const char *id, const char *field, const char *value)
{
	const char *items_path = def->items_path ? def->items_path : (const char *)store->user;
	return hyle_source_store_fs(items_path).ops->put_field(store, def, id, field, value);
}

static int tok_del(hyle_source_store_t *store, const hyle_source_def_t *def,
                   const char *id)
{
	const char *items_path = def->items_path ? def->items_path : (const char *)store->user;
	return hyle_source_store_fs(items_path).ops->del(store, def, id);
}

static const hyle_source_store_ops_t tok_ops = {
	.scan = tok_scan,
	.load = tok_load,
	.put = tok_put,
	.put_field = tok_put_field,
	.del = tok_del,
};

const hyle_source_store_ops_t *hyle_source_store_tok_ops(void) { return &tok_ops; }

hyle_source_store_t hyle_source_store_tok(const char *items_path)
{
	hyle_source_store_t s = { &tok_ops, (void *)items_path };
	return s;
}
