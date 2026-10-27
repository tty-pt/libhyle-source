# libhyle-source

[![C99](https://img.shields.io/badge/C-C99-555?logo=c)](#)
[![BSD-2-Clause](https://img.shields.io/badge/License-BSD--2--Clause-blue)](#)
[![Pluggable drivers](https://img.shields.io/badge/storage-pluggable-16A34A)](#)

> Standalone persistence engine for Hyle datasets: dataset definition, CRUD via pluggable storage drivers, validation, full-text querying, ordered collections, and state overlays. Canonical header is `<hyle-source/hyle_source.h>`.

---

## 1. Quickstart

Register a dataset, persist a record, and run full-text search:

```c
#include <hyle-source/hyle_source.h>
#include <stdio.h>
#include <string.h>

static const hyle_source_field_t album_fields[] = {
    { .name = "title", .file = "title", .type = HYLE_FIELD_STRING, .writable = 1, .required = 1 },
    { .name = "year",  .file = "year",  .type = HYLE_FIELD_INT,    .writable = 1 },
};

static int get_single(const char *name, char *buf, size_t sz, void *user) {
    (void)user;
    if (strcmp(name, "title") == 0) return snprintf(buf, sz, "Ain't Talkin' 'Bout Love");
    if (strcmp(name, "year") == 0)  return snprintf(buf, sz, "1995");
    return -1;
}

int main(void) {
    hyle_source_def_t def = {
        .id = "album",
        .key_field = "id",
        .items_path = "album",
        .fields = album_fields,
        .field_count = 2,
        .record_size = sizeof(album_fields),
    };

    if (hyle_source_register_def(&def) != 0) return 1;

    // Parse form/request data into corm record and persist
    unsigned hd = hyle_source_parse_row_data_custom(&def, get_single, NULL, NULL);
    if (hyle_source_update_item(0, "album", "album_dance", hd) != 0) return 2;

    // Read stored field
    const char *title = hyle_source_get_field("album", "album_dance", "title");
    printf("Stored: %s\n", title ? title : "(null)");

    // Full-text search over dataset
    unsigned qh = hyle_source_query_dataset("album", "love");
    const char *total = qh ? corm_get(qh, "__total__") : NULL;
    printf("Search 'love': %s match(es)\n", total ? total : "0");
    return 0;
}
```

Compile and link:
```sh
cc dataset_demo.c $(pkg-config --cflags --libs hyle-source) -o dataset_demo
```

---

## 2. Storage Drivers

`hyle_source_store_ops_t` abstracts persistence behind five operations (`scan`, `load`, `put`, `put_field`, `del`):

| Driver | Storage Target | Characteristics | Usage |
| :--- | :--- | :--- | :--- |
| `hyle_source_store_fs` | Local filesystem | Persistent, human-readable JSON files, Git-versionable | Production site datasets, content catalogs |
| `hyle_source_store_tok` | Binary token container | Lossless subword BPE tokens (`data.tok`/`record.tok`) | High-density text archives, fast scanning |
| `hyle_source_store_mem` | Volatile RAM | In-memory hash store, zero disk I/O | Fast test suites, temporary session datasets |

*The bundled `hyle-tok` CLI tool allows inspecting, dumping, and validating binary token containers.*

---

## 3. Core API Reference

| Function | Signature | Description |
| :--- | :--- | :--- |
| `hyle_source_setup` | `int hyle_source_setup(id, key_field, record_size, ...)` | Bootstrap a complete dataset definition |
| `hyle_source_register_def` | `int hyle_source_register_def(const hyle_source_def_t *def)` | Wire dataset into Hyle registry and FTS index |
| `hyle_source_update_item` | `int hyle_source_update_item(auth, ds, id, hd)` | Persist or update record via storage driver |
| `hyle_source_delete_item` | `int hyle_source_delete_item(auth, ds, id)` | Delete record and update secondary indexes |
| `hyle_source_query_dataset` | `unsigned hyle_source_query_dataset(ds, query_str)` | FTS search + filter + sort + pagination query |
| `hyle_source_get_field` | `const char *hyle_source_get_field(ds, id, field)` | Retrieve string field value directly |
| `hyle_source_validate_row` | `int hyle_source_validate_row(def, hd, &err_json)` | Validate record constraints against schema |
| `hyle_source_build_state_json`| `char *hyle_source_build_state_json(...)` | Serialize records for SSR/WASM state overlays |

---

## 4. Installation & Build

### Prebuilt Packages
Prebuilt packages are distributed on tty.pt for Linux (APT / Alpine / Arch / Fedora-RHEL), macOS (Homebrew), Windows (winget / MSYS2), and OpenBSD. Follow the [installation instructions](https://github.com/tty-pt/ci/blob/main/docs/install.md) and use **libhyle-source** as the package name.

### Build from Source
```sh
make                  # builds lib/libhyle-source.so and bin/hyle-tok
make test             # run verification test suite
sudo make install     # install headers, tool, and library to system
```

## License

BSD 2-Clause License. Copyright (c) 2026, tty-pt. See `LICENSE`.
