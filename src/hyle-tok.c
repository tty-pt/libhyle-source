#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ttypt/lemma.h>

#define HTOK_MAGIC 0x48544F4B

static int do_pack_dir(const char *dir_path, const char *out_path)
{
	DIR *d = opendir(dir_path);
	if (!d) {
		fprintf(stderr, "hyle-tok: cannot open dir %s\n", dir_path);
		return 1;
	}

	struct dirent *de;
	char field_names[64][64];
	uint16_t *field_tokens[64];
	size_t field_token_counts[64];
	size_t n_fields = 0;

	while ((de = readdir(d)) != NULL) {
		if (de->d_name[0] == '.' || strstr(de->d_name, ".tok") != NULL)
			continue;
		char fpath[4096];
		snprintf(fpath, sizeof(fpath), "%s/%s", dir_path, de->d_name);
		struct stat st;
		if (stat(fpath, &st) != 0 || !S_ISREG(st.st_mode))
			continue;

		FILE *fp = fopen(fpath, "rb");
		if (!fp)
			continue;
		fseek(fp, 0, SEEK_END);
		long sz = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		char *buf = malloc(sz + 1);
		if (!buf || fread(buf, 1, sz, fp) != (size_t)sz) {
			fclose(fp);
			free(buf);
			continue;
		}
		buf[sz] = '\0';
		fclose(fp);

		uint16_t *tokens = malloc((sz + 1) * sizeof(uint16_t));
		size_t n_tok = 0;
		if (lemma_tok_encode(NULL, buf, sz, tokens, sz + 1, &n_tok) == 0 && n_fields < 64) {
			strncpy(field_names[n_fields], de->d_name, 63);
			field_names[n_fields][63] = '\0';
			field_tokens[n_fields] = tokens;
			field_token_counts[n_fields] = n_tok;
			n_fields++;
		} else {
			free(tokens);
		}
		free(buf);
	}
	closedir(d);

	FILE *out = fopen(out_path, "wb");
	if (!out) {
		for (size_t i = 0; i < n_fields; i++)
			free(field_tokens[i]);
		fprintf(stderr, "hyle-tok: cannot open output %s\n", out_path);
		return 1;
	}

	uint32_t magic = HTOK_MAGIC;
	uint16_t version = 1;
	uint16_t fc = (uint16_t)n_fields;

	fwrite(&magic, 4, 1, out);
	fwrite(&version, 2, 1, out);
	fwrite(&fc, 2, 1, out);

	for (size_t i = 0; i < n_fields; i++) {
		uint8_t nlen = (uint8_t)strlen(field_names[i]);
		fwrite(&nlen, 1, 1, out);
		fwrite(field_names[i], 1, nlen, out);

		uint16_t tc = (uint16_t)field_token_counts[i];
		fwrite(&tc, 2, 1, out);
		fwrite(field_tokens[i], 2, tc, out);
		free(field_tokens[i]);
	}
	fclose(out);

	printf("hyle-tok: packed directory %s (%zu fields) -> %s\n", dir_path, n_fields, out_path);
	return 0;
}

static int do_pack(const char *in_path, const char *out_path)
{
	FILE *in = fopen(in_path, "rb");
	if (!in) {
		fprintf(stderr, "hyle-tok: cannot open input %s\n", in_path);
		return 1;
	}
	fseek(in, 0, SEEK_END);
	long sz = ftell(in);
	fseek(in, 0, SEEK_SET);

	char *buf = malloc(sz + 1);
	if (!buf || fread(buf, 1, sz, in) != (size_t)sz) {
		fclose(in);
		free(buf);
		fprintf(stderr, "hyle-tok: read error\n");
		return 1;
	}
	buf[sz] = '\0';
	fclose(in);

	uint16_t *tokens = malloc((sz + 1) * sizeof(uint16_t));
	size_t n_tokens = 0;
	if (lemma_tok_encode(NULL, buf, sz, tokens, sz + 1, &n_tokens) != 0) {
		free(buf);
		free(tokens);
		fprintf(stderr, "hyle-tok: encode error\n");
		return 1;
	}
	free(buf);

	FILE *out = fopen(out_path, "wb");
	if (!out) {
		free(tokens);
		fprintf(stderr, "hyle-tok: cannot open output %s\n", out_path);
		return 1;
	}

	uint32_t magic = HTOK_MAGIC;
	uint16_t version = 1;
	uint32_t tok_count = (uint32_t)n_tokens;

	fwrite(&magic, 4, 1, out);
	fwrite(&version, 2, 1, out);
	fwrite(&tok_count, 4, 1, out);
	fwrite(tokens, 2, n_tokens, out);
	fclose(out);
	free(tokens);

	printf("hyle-tok: packed %ld bytes -> %zu tokens (%zu bytes)\n",
	       sz, n_tokens, n_tokens * 2 + 10);
	return 0;
}

static int do_unpack(const char *in_path, const char *out_path)
{
	FILE *in = fopen(in_path, "rb");
	if (!in) {
		fprintf(stderr, "hyle-tok: cannot open input %s\n", in_path);
		return 1;
	}

	uint32_t magic = 0;
	uint16_t version = 0;
	uint32_t tok_count = 0;

	if (fread(&magic, 4, 1, in) != 1 || magic != HTOK_MAGIC) {
		fclose(in);
		fprintf(stderr, "hyle-tok: invalid magic\n");
		return 1;
	}
	if (fread(&version, 2, 1, in) != 1 || fread(&tok_count, 4, 1, in) != 1) {
		fclose(in);
		fprintf(stderr, "hyle-tok: header read error\n");
		return 1;
	}

	uint16_t *tokens = malloc(tok_count * sizeof(uint16_t));
	if (!tokens || fread(tokens, 2, tok_count, in) != tok_count) {
		fclose(in);
		free(tokens);
		fprintf(stderr, "hyle-tok: token read error\n");
		return 1;
	}
	fclose(in);

	size_t cap = tok_count * 16 + 256;
	char *out_buf = malloc(cap);
	size_t bytes_written = 0;

	if (lemma_tok_decode(NULL, tokens, tok_count, out_buf, cap, &bytes_written) != 0) {
		free(tokens);
		free(out_buf);
		fprintf(stderr, "hyle-tok: decode error\n");
		return 1;
	}
	free(tokens);

	FILE *out = fopen(out_path, "wb");
	if (!out) {
		free(out_buf);
		fprintf(stderr, "hyle-tok: cannot open output %s\n", out_path);
		return 1;
	}
	fwrite(out_buf, 1, bytes_written, out);
	fclose(out);
	free(out_buf);

	printf("hyle-tok: unpacked %u tokens -> %zu bytes\n", tok_count, bytes_written);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 4) {
		fprintf(stderr, "Usage:\n  hyle-tok pack <input.txt> <output.tok>\n  hyle-tok unpack <input.tok> <output.txt>\n");
		return 1;
	}

	if (strcmp(argv[1], "pack") == 0)
		return do_pack(argv[2], argv[3]);
	else if (strcmp(argv[1], "unpack") == 0)
		return do_unpack(argv[2], argv[3]);
	else if (strcmp(argv[1], "pack-dir") == 0)
		return do_pack_dir(argv[2], argv[3]);

	fprintf(stderr, "Unknown command: %s\n", argv[1]);
	return 1;
}
