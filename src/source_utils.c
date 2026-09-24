#include "source_utils.h"
#include <hyle-source/hyle_source.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <iconv.h>
#include <locale.h>

static iconv_t slug_cd = (iconv_t)-1;
static int slug_init_done = 0;

static void slug_init(void)
{
	if (slug_init_done)
		return;
	slug_init_done = 1;
	slug_cd = iconv_open("ASCII//TRANSLIT", "UTF-8");
}

int source_util_is_safe_id(const char *id)
{
	const char *p;
	if (!id || !id[0])
		return 0;
	for (p = id; *p; p++) {
		char c = *p;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		    (c >= '0' && c <= '9') || c == '_' || c == '-')
			continue;
		return 0;
	}
	return 1;
}

char *source_util_slurp_file(const char *path)
{
	FILE *fp = fopen(path, "rb");
	if (!fp)
		return NULL;

	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return NULL;
	}
	long sz = ftell(fp);
	if (sz < 0) {
		fclose(fp);
		return NULL;
	}
	rewind(fp);

	char *buf = malloc((size_t)sz + 1);
	if (!buf) {
		fclose(fp);
		return NULL;
	}

	size_t read_bytes = fread(buf, 1, (size_t)sz, fp);
	buf[read_bytes] = '\0';
	fclose(fp);
	return buf;
}

int source_util_write_file(const char *path, const char *buf, size_t sz)
{
	char tmp_path[PATH_MAX];
	int fd;
	ssize_t written;

	if (!path || !path[0])
		return -1;

	snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%d", path, (int)getpid());

	fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0644);
	if (fd < 0)
		return -1;

	if (sz > 0) {
		written = write(fd, buf, sz);
		if (written < 0 || (size_t)written != sz) {
			close(fd);
			unlink(tmp_path);
			return -1;
		}
	}

	if (fsync(fd) != 0) {
		close(fd);
		unlink(tmp_path);
		return -1;
	}

	if (close(fd) != 0) {
		unlink(tmp_path);
		return -1;
	}

	if (rename(tmp_path, path) != 0) {
		unlink(tmp_path);
		return -1;
	}

	return 0;
}

int hyle_source_is_safe_id(const char *id)
{
	return source_util_is_safe_id(id);
}

char *hyle_source_slurp_file(const char *path)
{
	return source_util_slurp_file(path);
}

int hyle_source_write_file(const char *path, const char *buf, size_t sz)
{
	return source_util_write_file(path, buf, sz);
}

int hyle_source_remove_path_recursive(const char *path)
{
	return source_util_remove_path_recursive(path);
}

const char *hyle_source_resolve_doc_root(char *buf, size_t sz)
{
	return source_util_resolve_doc_root(buf, sz);
}

int source_util_remove_path_recursive(const char *path)
{
	struct stat st;
	if (lstat(path, &st) != 0)
		return -1;

	if (S_ISDIR(st.st_mode)) {
		DIR *d = opendir(path);
		if (!d)
			return -1;
		struct dirent *de;
		while ((de = readdir(d))) {
			if (strcmp(de->d_name, ".") == 0 ||
			    strcmp(de->d_name, "..") == 0)
				continue;
			char child[PATH_MAX];
			snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
			source_util_remove_path_recursive(child);
		}
		closedir(d);
		return rmdir(path);
	}
	return unlink(path);
}

const char *source_util_resolve_doc_root(char *buf, size_t sz)
{
	const char *env_root = getenv("DOCUMENT_ROOT");
	if (env_root && env_root[0]) {
		if (buf && sz) {
			snprintf(buf, sz, "%s", env_root);
			return buf;
		}
		return env_root;
	}
	if (buf && sz) {
		snprintf(buf, sz, ".");
		return buf;
	}
	return ".";
}

int source_util_slugify(const char *title, size_t title_len,
                       char *result, size_t result_len)
{
	size_t written;
	char *r_ptr = result;
	char *w_ptr = result;
	char *in;
	char *out;
	size_t in_len;
	size_t out_len;
	size_t i;

	if (!title || !result || result_len == 0)
		return -1;

	slug_init();

	if (slug_cd != (iconv_t)-1) {
		in = (char *)title;
		in_len = title_len;
		out = result;
		out_len = result_len - 1;

		while (in_len > 0 && out_len > 0) {
			size_t res =
			        iconv(slug_cd, (void *)&in, &in_len, &out, &out_len);
			if (res != (size_t)-1)
				continue;
			if (errno != EILSEQ && errno != EINVAL)
				break;
			in++;
			in_len--;
			iconv(slug_cd, NULL, NULL, &out, &out_len);
		}
		written = (size_t)(out - result);
	} else {
		size_t to_copy =
		        title_len < result_len ? title_len : result_len - 1;
		memcpy(result, title, to_copy);
		written = to_copy;
	}

	for (i = 0; i < written; i++) {
		char c = r_ptr[i];
		if (c == ' ')
			*w_ptr++ = '_';
		else if (c >= 'A' && c <= 'Z')
			*w_ptr++ = c + 32;
		else if ((c >= 'a' && c <= 'z') ||
		         (c >= '0' && c <= '9') || c == '_')
			*w_ptr++ = c;
	}
	*w_ptr = '\0';

	if (w_ptr == result)
		snprintf(result, result_len, "item");

	return 0;
}
