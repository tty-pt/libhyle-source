# libhyle-source

[![C99](https://img.shields.io/badge/C-C99-555?logo=c)](#)
[![BSD-2-Clause](https://img.shields.io/badge/License-BSD--2--Clause-blue)](#)
[![Pluggable drivers](https://img.shields.io/badge/storage-pluggable-16A34A)](#)

The standalone persistence engine for Hyle datasets: dataset definition and
registration, record validation, CRUD through pluggable storage drivers,
full-text querying, ordered DSV collections, metadata JSON, and JSON state
overlays for SSR/WASM hydration. It keeps storage mechanics completely separate
from the data schemas and query machinery in `libhyle`.

---

## Contents

- [Features](#features)
- [Build & install](#build--install)
- [Quickstart](#quickstart)
- [API overview](#api-overview)
- [Consumers](#consumers)
- [Documentation](#documentation)
- [Testing](#testing)
- [License](#license)

## Features

**Dataset definitions** — `hyle_source_def_t` describes a dataset: `id`,
`key_field`, `items_path`, fields (`hyle_source_field_t`), an access policy
(`PUBLIC` / `LOGIN`), a list view, and the storage driver. `hyle_source_register_def`
registers it and wires it into the Hyle registry (fields + row maps + FTS index);
`hyle_source_setup` bootstraps a full dataset (defs, key field, record size,
items path, flags, list view) in one call.

**Pluggable storage drivers** — `hyle_source_store_ops_t` abstracts persistence
behind five operations (`scan`, `load`, `put`, `put_field`, `del`). Two drivers
ship out of the box: `hyle_source_store_fs` (per-item directories under
`DOCUMENT_ROOT/items_path/<id>/`) and `hyle_source_store_mem` (volatile stub for
tests). When a def leaves `store` unset, `register_def` injects the filesystem
driver — or the in-memory one for `HYLE_SOURCE_FLAG_VOLATILE` datasets.

**Validation** — `hyle_source_validate_row` enforces required fields, int
`min`/`max`, `min_length`/`max_length`, and regex `pattern` rules from the field
schema, returning a JSON error list; `HYLE_SOURCE_ERR_VALIDATION` marks a failed
write.

**Query & full-text search** — `hyle_source_query_dataset` runs the shared
query pipeline (full-text substring search across id + text fields, per-field
filters, sort, pagination) and returns a row handle with a `__total__` entry;
`hyle_source_get_desc` / `_get_data_hd` / `_get_fields_hd` / `_get_schema_hd`
expose the underlying registry handles.

**Ordered datasets** — `hyle_source_dsv_load` / `hyle_source_dsv_save` back
ordered DSV collections (`data.txt` partitions) with a self-persisting
high-level API: `hyle_source_ordered_append_and_save`, `_set_field`,
`_remove_and_save`, `_remove_matching`, `_replace_row`, `_find`,
`_for_each`, and `hyle_source_ordered_sync_form_custom` for form-driven edits.

**Metadata & state overlays** — `hyle_source_meta_read` / `_meta_write` move
record metadata to `var/` JSON; `hyle_source_build_state_json` /
`_build_state_specs` / `_build_item_json` serialize records for SSR,
`hyle_source_state_overlay` / `_overlay_from_desc` / `_overlay_array` fold C
structs into JSON state (same zero-copy stride contract as the Bud bridge), and
`hyle_json_extract_strings` pulls values back out.

**References & options** — `hyle_source_resolve_ref_display_str`,
`_get_display_field`, `_get_item_label` resolve reference displays;
`hyle_source_resolve_options`, `_resolve_tokens`, `_normalize_tokens_to_slugs`,
and `_get_enum_options` build the option pools that feed picker dropdowns;
`hyle_source_find_referencing` / `_for_each_referencing`,
`hyle_source_inv_keys` / `_inv_key_at`, and `hyle_source_clear_inverse_refs`
handle inverse-relation bookkeeping.

**Form parsing & file utilities** — `hyle_source_parse_row_data_custom`
rehydrates submitted form data into a qmap handle via `hyle_field_getter_fn` /
`hyle_multi_field_getter_fn` callbacks; `hyle_source_get_field(_int)` /
`set_field(_int)` give ergonomic record access; `hyle_source_is_safe_id`,
`_slurp_file`, `_write_file`, `_remove_path_recursive`, `_resolve_doc_root` back
the filesystem layer.

**Picker DTO ownership** — libhyle-source is the canonical home of the picker
presentation types (`hyle_option_t`, `hyle_picker_desc_t`, `hyle_picker_entry_t`,
`hyle_picker_view_t`, `hyle_picker_buffer_t`) in `<hyle-source/picker.h>`, plus
the `HYLE_PICKER_*` capacity constants. `libhyle-bud` consumes them directly.

## Build & install

```sh
cd external/libhyle-source
make          # lib/libhyle-source.so

sudo make install   # lib, headers, and hyle-source.pc → $(PREFIX), default /usr/local
```

Link it from your own C code:

```sh
cc my_app.c $(pkg-config --cflags --libs hyle-source)
```

`hyle-source.pc` carries the dependency chain
(`-lhyle-source -lhyle -lqmap -lstoma -ljson-c`; Darwin adds `-liconv`).

**Dependencies:** `external/libhyle` (schemas + registry/query),
`external/libqmap`, `external/stoma` (full-text search), and `json-c`.
No framework, no network, no UI.

## Quickstart

The example stores a record behind the filesystem driver and searches it. The
filesystem store resolves the root from `DOCUMENT_ROOT` (current directory when
unset), so point it at a writable scratch dir before running.

```c
#include <hyle-source/hyle_source.h>
#include <stdio.h>
#include <string.h>

static const hyle_source_field_t album_fields[] = {
	{ .name = "title", .file = "title", .type = HYLE_FIELD_STRING, .writable = 1, .required = 1 },
	{ .name = "year",  .file = "year",  .type = HYLE_FIELD_INT,    .writable = 1 },
};

static int get_single(const char *name, char *buf, size_t sz, void *user)
{
	(void)user;
	if (strcmp(name, "title") == 0)
		return snprintf(buf, sz, "Ain't Talkin' 'Bout Love");
	if (strcmp(name, "year") == 0)
		return snprintf(buf, sz, "1995");
	return -1;
}

static int get_multi(const char *name, char *buf, size_t sz, void *user)
{
	(void)name; (void)buf; (void)sz; (void)user;
	return 0;
}

int main(void)
{
	hyle_source_def_t def = {
		.id = "album",
		.key_field = "id",
		.items_path = "album",
		.fields = album_fields,
		.field_count = 2,
		.record_size = sizeof(album_fields),
	};

	if (hyle_source_register_def(&def) != 0)
		return 1;

	/* Build a row from form-field getters, then persist it. */
	unsigned hd = hyle_source_parse_row_data_custom(
	        &def, get_single, get_multi, NULL);

	if (hyle_source_update_item(0, "album", "album_dance", hd) != 0)
		return 2;

	const char *title = hyle_source_get_field("album", "album_dance", "title");
	printf("stored: %s\n", title ? title : "(null)");

	/* Full-text search over id + string fields. */
	unsigned qh = hyle_source_query_dataset("album", "love");
	const char *total = qh ? qmap_get(qh, "__total__") : NULL;
	printf("search \"love\": %s match%s\n",
	       total ? total : "0", total && strcmp(total, "1") ? "es" : "");
	return 0;
}
```

## API overview

Full signatures live in `include/hyle-source/*.h` (`hyle_source.h`, `store.h`,
`picker.h`).

**Registration & dataset access** — `hyle_source_setup`, `hyle_source_register_def`,
`hyle_source_find`, `hyle_source_for_each`, `hyle_source_is_creatable`,
`hyle_source_get_desc`, `hyle_source_get_record_size`,
`hyle_source_get_data_hd` / `_get_fields_hd` / `_get_schema_hd`,
`hyle_source_get_list_view`.

**CRUD & validation** — `hyle_source_validate_row`, `hyle_source_update_item`,
`hyle_source_delete_item`, `hyle_source_refresh_row`,
`hyle_source_item_exists`, `hyle_source_clear_inverse_refs`,
`hyle_source_get_field(_int)` / `hyle_source_set_field(_int)`.

**Querying** — `hyle_source_query_dataset`, `hyle_qmap_get_field_str`.

**Storage drivers** — `hyle_source_store_ops_t`, `hyle_source_store_fs`,
`hyle_source_store_fs_ops`, `hyle_source_store_mem`,
`hyle_source_store_mem_ops`.

**Ordered datasets** — `hyle_source_dsv_load`, `hyle_source_dsv_save`,
`hyle_source_ordered_get_field` / `_set_field` / `_append_and_save` /
`_remove_and_save` / `_remove_matching` / `_replace_row` / `_find` /
`_for_each`, `hyle_source_ordered_sync_form_custom`
(`hyle_ordered_field_sync_t`).

**Metadata & overlays** — `hyle_source_build_state_specs`,
`hyle_source_build_state_json`, `hyle_source_build_item_json`,
`hyle_source_state_overlay` (`hyle_source_state_kv_t`),
`hyle_source_overlay_from_desc`, `hyle_source_overlay_array`,
`hyle_source_meta_read`, `hyle_source_meta_write`,
`hyle_source_resolve_meta_display`, `hyle_json_extract_strings`
(`hyle_json_str_map_t`).

**References & options** — `hyle_source_resolve_ref_display_str`,
`hyle_source_get_display_field`, `hyle_source_get_item_label`,
`hyle_source_resolve_options`, `hyle_source_resolve_tokens`,
`hyle_source_normalize_tokens_to_slugs`, `hyle_source_get_enum_options`,
`hyle_source_find_referencing`, `hyle_source_for_each_referencing`,
`hyle_source_inv_keys`, `hyle_source_inv_key_at`, `hyle_ref_field_register`.

**Forms & files** — `hyle_source_parse_row_data_custom`
(`hyle_field_getter_fn`, `hyle_multi_field_getter_fn`),
`hyle_source_is_safe_id`, `hyle_source_slurp_file`,
`hyle_source_write_file`, `hyle_source_remove_path_recursive`,
`hyle_source_resolve_doc_root`,
`hyle_source_internal_process_multi_ref`.

**Picker DTOs** — `<hyle-source/picker.h>`: `hyle_option_t`,
`hyle_picker_desc_t`, `hyle_picker_entry_t`, `hyle_picker_view_t`,
`hyle_picker_buffer_t`, `HYLE_PICKER_MAX_OPTS`, `HYLE_PICKER_MAX_SEL`,
`HYLE_PICKER_MAX_FIELDS`, `HYLE_PICKER_QS_BUDGET`.

## Consumers

| Target | Where |
|--------|-------|
| Bud UI bridge (pickers, filters, forms) | `../libhyle-bud/` |
| Site data layer (`source` module, handlers) | site modules |
| Picker presentation contract | `../../docs/PICKERS.md` |
| Schema hint contract | `../../docs/SCHEMA.md` |

## Documentation

- `../../docs/ARCHITECTURE.md` — where the persistence engine sits in the module graph
- `../../docs/SCHEMA.md` — schema hint strings (`filter_style`, `filter_mode`, `allow_add`)
- `../../docs/FILTERS.md` — the query/filter contract shared with consumers
- `../../docs/PICKERS.md` — how the option pools feed the omni-dropdowns
- `../../docs/C-ISOMORPHIC-BUD.md` — state overlays and hydration

## Testing

libhyle-source ships no standalone test binary; the filesystem and in-memory
drivers, ordered DSV paths, and overlay builders are exercised by the site
suite at the repository root (`make test` — SSR plus E2E), and `make
boundary-check` enforces module-layer rules.

## License

BSD 2-Clause License. Copyright (c) 2026, tty-pt. See `../../LICENSE`.