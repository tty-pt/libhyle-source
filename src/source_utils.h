#ifndef SOURCE_UTILS_H
#define SOURCE_UTILS_H

#include <stddef.h>
#include <stdint.h>

int source_util_is_safe_id(const char *id);
char *source_util_slurp_file(const char *path);
int source_util_write_file(const char *path, const char *buf, size_t sz);
int source_util_remove_path_recursive(const char *path);
const char *source_util_resolve_doc_root(char *buf, size_t sz);
int source_util_slugify(const char *title, size_t title_len, char *result, size_t result_len);

#endif
