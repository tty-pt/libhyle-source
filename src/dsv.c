#include "hyle-source/hyle_source.h"
#include "source_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <ttypt/corm.h>
#include <ttypt/lemma.h>
#include <hyle/registry.h>

#define HTOK_MAGIC 0x48544F4B

static void dsv_path(
        char *buf, size_t sz, const char *doc_root, const char *source_id,
        const char *pval)
{
	const char *dot = strchr(source_id, '.');
	if (dot) {
		size_t plen = (size_t)(dot - source_id);
		snprintf(
		        buf, sz, "%s/var/%.*s/%s/data.txt", doc_root, (int)plen,
		        source_id, pval);
	} else {
		snprintf(
		        buf, sz, "%s/var/%s/%s/data.txt", doc_root, source_id,
		        pval);
	}
}

static void dsv_tok_path(
        char *buf, size_t sz, const char *doc_root, const char *source_id,
        const char *pval)
{
	const char *dot = strchr(source_id, '.');
	if (dot) {
		size_t plen = (size_t)(dot - source_id);
		snprintf(
		        buf, sz, "%s/var/%.*s/%s/data.tok", doc_root, (int)plen,
		        source_id, pval);
	} else {
		snprintf(
		        buf, sz, "%s/var/%s/%s/data.tok", doc_root, source_id,
		        pval);
	}
}

int hyle_source_dsv_load(
        const char *source_id,
        const char *pval,
        unsigned fhd,
        void *user)
{
	const char *doc_root = (const char *)user;
	char dpath[PATH_MAX];
	char tok_path[PATH_MAX];
	char *raw = NULL;
	char *p;
	size_t nfields, j;
	int pos = 0;

	(void)fhd;
	if (!doc_root)
		return -1;

	/* Check for companion binary data.tok first */
	dsv_tok_path(tok_path, sizeof(tok_path), doc_root, source_id, pval);
	FILE *tfp = fopen(tok_path, "rb");
	if (tfp) {
		uint32_t magic = 0;
		uint16_t version = 0;
		uint32_t tok_count = 0;

		if (fread(&magic, 4, 1, tfp) == 1 && magic == HTOK_MAGIC &&
		    fread(&version, 2, 1, tfp) == 1 &&
		    fread(&tok_count, 4, 1, tfp) == 1) {
			uint16_t *tokens = malloc(tok_count * sizeof(uint16_t));
			if (tokens && fread(tokens, 2, tok_count, tfp) == tok_count) {
				size_t cap = tok_count * 16 + 256;
				char *out_buf = malloc(cap);
				size_t bw = 0;
				if (out_buf && lemma_tok_decode(NULL, tokens, tok_count, out_buf, cap, &bw) == 0) {
					fclose(tfp);
					free(tokens);
					raw = out_buf;
					goto parse_dsv_raw;
				}
				free(out_buf);
			}
			free(tokens);
		}
		fclose(tfp);
	}

	dsv_path(dpath, sizeof(dpath), doc_root, source_id, pval);
	raw = source_util_slurp_file(dpath);
	if (!raw)
		return 0;

parse_dsv_raw:
	nfields = hyle_registry_get_field_count(source_id);

	p = raw;
	while (*p) {
		char *nl = strchr(p, '\n');
		size_t llen = nl ? (size_t)(nl - p) : strlen(p);
		if (llen > 0 && p[0] != '#') {
			const char *names[128];
			const char *vals[128];
			char line[2048];
			char *parts[128];
			int nparts = 0, vi;
			char *cur;

			snprintf(line, sizeof(line), "%.*s", (int)llen, p);
			cur = line;
			parts[nparts++] = cur;
			while (*cur && nparts < (int)nfields) {
				if (*cur == ':') {
					*cur = '\0';
					parts[nparts++] = cur + 1;
				}
				cur++;
			}
			if (nparts == (int)nfields ||
			    nparts == (int)nfields - 1)
			{
				char key[128];
				vi = 0;
				for (j = 0; j < nfields && vi < nparts; j++) {
					if (hyle_registry_get_field_type(
					            source_id, j) ==
					    HYLE_FIELD_INVERSE)
						continue;
					names[vi] = hyle_registry_get_field_name(
					        source_id, j);
					vals[vi] = parts[vi];
					vi++;
				}
				snprintf(
				        key, sizeof(key), "%s__%04d", pval,
				        pos);
				hyle_registry_put(
				        source_id, key, names, vals, vi);
				pos++;
			}
		}
		p = nl ? nl + 1 : p + strlen(p);
	}
	free(raw);
	return 0;
}

int hyle_source_dsv_save(
        const char *source_id,
        const char *pval,
        unsigned fhd,
        void *user)
{
	const char *doc_root = (const char *)user;
	char dpath[PATH_MAX], tmp[PATH_MAX + 4];
	int n, i;
	FILE *fp;
	size_t nfields, j;

	if (!doc_root)
		return -1;

	dsv_path(dpath, sizeof(dpath), doc_root, source_id, pval);
	snprintf(tmp, sizeof(tmp), "%s.tmp", dpath);
	fp = fopen(tmp, "w");
	if (!fp)
		return -1;

	nfields = hyle_registry_get_field_count(source_id);
	n = hyle_ordered_count(source_id, pval);
	for (i = 0; i < n; i++) {
		const char *key;
		int first;

		key = hyle_ordered_key_at(source_id, pval, i);
		if (!key)
			continue;
		first = 1;
		for (j = 0; j < nfields; j++) {
			const char *val;

			if (hyle_registry_get_field_type(source_id, j) ==
			    HYLE_FIELD_INVERSE)
				continue;
			val = corm_field_get(
			        fhd, key,
			        hyle_registry_get_field_name(source_id, j));
			if (first) {
				fprintf(fp, "%s", val ? val : "");
				first = 0;
			} else {
				fprintf(fp, ":%s", val ? val : "");
			}
		}
		fprintf(fp, "\n");
	}
	fclose(fp);
	rename(tmp, dpath);

	/* Sync companion data.tok binary container */
	char tok_path[PATH_MAX], tok_tmp[PATH_MAX + 4];
	dsv_tok_path(tok_path, sizeof(tok_path), doc_root, source_id, pval);
	snprintf(tok_tmp, sizeof(tok_tmp), "%s.tmp", tok_path);
	FILE *tfp = fopen(tok_tmp, "wb");
	if (tfp) {
		char *raw_saved = source_util_slurp_file(dpath);
		if (raw_saved) {
			size_t rlen = strlen(raw_saved);
			uint16_t *tokens = malloc((rlen + 1) * sizeof(uint16_t));
			size_t n_tokens = 0;
			if (tokens && lemma_tok_encode(NULL, raw_saved, rlen, tokens, rlen + 1, &n_tokens) == 0) {
				uint32_t magic = HTOK_MAGIC;
				uint16_t version = 1;
				uint32_t tc = (uint32_t)n_tokens;
				fwrite(&magic, 4, 1, tfp);
				fwrite(&version, 2, 1, tfp);
				fwrite(&tc, 4, 1, tfp);
				fwrite(tokens, 2, n_tokens, tfp);
				fclose(tfp);
				rename(tok_tmp, tok_path);
			} else {
				fclose(tfp);
				unlink(tok_tmp);
			}
			free(tokens);
			free(raw_saved);
		} else {
			fclose(tfp);
			unlink(tok_tmp);
		}
	}
	return 0;
}
