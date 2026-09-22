# libhyle-source — Standalone Dataset Persistence Engine

Modular C persistence layer providing dataset registration, record CRUD, schema validation, serialization, and pluggable storage drivers for Hyle.

## Overview

`libhyle-source` separates storage mechanics from data schemas and queries:
- **Pluggable Drivers (`hyle_source_store_ops_t`):** Filesystem storage (`store_fs`), in-memory storage (`store_mem`), or custom persistence engines.
- **Record Serialization:** Automated conversion between C structs, JSON overlays, and filesystem layouts based on `hyle_schema_desc_t`.
- **Validation Engine:** Validates required fields, string lengths, and schema invariants on writes.
- **Ordered Datasets:** Dedicated APIs for ordered DSV collection lists (`data.txt`).

## Storage Operations (`hyle-source/store.h`)

```c
typedef struct {
    int (*scan)(hyle_source_store_t *store, const struct hyle_source_def_s *def);
    int (*load)(hyle_source_store_t *store, const struct hyle_source_def_s *def,
                const char *id, unsigned *row_out);
    int (*put)(hyle_source_store_t *store, const struct hyle_source_def_s *def,
               const char *id, unsigned data_handle);
    int (*put_field)(hyle_source_store_t *store,
                     const struct hyle_source_def_s *def, const char *id,
                     const char *field, const char *value);
    int (*del)(hyle_source_store_t *store, const struct hyle_source_def_s *def,
               const char *id);
} hyle_source_store_ops_t;
```

## Key APIs (`hyle-source/hyle_source.h`)

```c
/* Register dataset definition */
int hyle_source_register_def(const hyle_source_def_t *def);

/* Update record and invalidate search index */
int hyle_source_update_item(int fd, const char *dataset_id,
                            const char *id, unsigned data_handle);

/* Delete record */
int hyle_source_delete_item(int fd, const hyle_source_def_t *def, const char *item_id);

/* Execute query with FTS + filters */
unsigned hyle_source_query_dataset(const char *dataset_id, const char *query_str);
```

## Dependencies

- `external/hyle` — Core query and schema definitions
- `external/libqmap` — Key-value registry
- `external/stoma` — FTS indexing
- `json-c` — JSON serialization
