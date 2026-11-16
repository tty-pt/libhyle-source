# @tty-pt/hyle-source

[![npm version](https://img.shields.io/npm/v/@tty-pt/hyle-source.svg)](https://www.npmjs.com/package/@tty-pt/hyle-source)
[![License: BSD-2-Clause](https://img.shields.io/badge/License-BSD--2--Clause-blue)](https://github.com/tty-pt/libhyle-source/blob/main/LICENSE)
[![TypeScript Ready](https://img.shields.io/badge/TypeScript-Ready-3178C6?logo=typescript)](#)
[![Node.js Native](https://img.shields.io/badge/Node.js-N--API-339933?logo=node.js)](#)
[![WebAssembly](https://img.shields.io/badge/WebAssembly-Reactor-654FF0?logo=webassembly)](#)
[![Zero Dependencies](https://img.shields.io/badge/dependencies-0-success)](#)

> Universal Node.js and Browser bindings for **`libhyle-source`**, the dataset persistence, pluggable storage driver (`fs`, `tok`, `mem`), full-text query, and optimistic LRU substrate cache engine for Hyle. Canonical C header: `<hyle-source/hyle_source.h>`.

📖 **Full C & Storage Driver Documentation**: See the main [**libhyle-source GitHub README**](https://github.com/tty-pt/libhyle-source#readme) for `hyle_source_store_ops_t` driver architecture, binary `.tok` containers (`hyle-tok`), and C dataset lifecycle functions.

---

## 1. Overview & Domain Concepts

`libhyle-source` (`@tty-pt/hyle-source`) is the persistence and query execution engine pairing with [`@tty-pt/hyle`](https://www.npmjs.com/package/@tty-pt/hyle) schemas:

1. **Pluggable Dataset Persistence & CRUD**:
   - Registers typed dataset schemas (`registerDataset`), persists and updates records (`updateItem`, `putItem`, `deleteItem`), and reads individual fields (`getField`).
2. **Accent-Sensitive Full-Text Search & Structured Queries (`queryDataset`)**:
   - Executes combined lexical search (via [`liblemma`](https://github.com/tty-pt/liblemma), preserving diacritics such as `pão ≠ pao`), field equality filtering, sorting, and pagination.
3. **In-Memory LRU Query & Normalized Entity Cache**:
   - Provides deterministic query normalization (`normalizeQuery`), query slice caching (`cachePut`, `cacheGet`, `cacheHydrate`, `cacheInvalidate`), and normalized entity records (`entityPut`, `entityGet`).
4. **Optimistic Mutations & Reactive Store Subscriptions**:
   - Supports zero-latency UI mutations (`optimisticUpdate` returning `{ commit, rollback }`) and `useSyncExternalStore`-compatible dataset subscriptions (`createHyleStore`, `subscribeDataset`).

---

## 2. Installation

```bash
npm install @tty-pt/hyle-source @tty-pt/hyle @tty-pt/corm
```

**Native OS Packages (`tty.pt`)**: Prebuilt native C packages (`libhyle-source`) for Linux (`apt`, `apk`, `pacman`, `dnf`), macOS (`brew`), Windows (`winget`, MSYS2), and OpenBSD (`pkg_add`) are available via the [**tty.pt package repositories**](https://github.com/tty-pt/ci/blob/main/docs/install.md).

---

## 3. Quickstart

```typescript
import {
  HyleSource,
  optimisticUpdate,
  slugify
} from "@tty-pt/hyle-source";

// 1. Initialize dataset persistence engine
const source = new HyleSource("./var/data");

source.registerDataset({
  id: "album",
  keyField: "id",
  fields: [
    { name: "title", type: "string", required: true },
    { name: "year", type: "int" }
  ]
});

// 2. Persist records and execute full-text search
source.putItem("album", "album_1", { title: "Canção do Mar", year: 1955 });
const results = source.query("album", "q=canção&sort=year&order=desc");
console.log("Matches:", results.total, results.items);

// 3. Optimistic UI cache update with commit / rollback handle
source.cacheHydrate("album", "active=1", [{ id: "album_1", title: "Canção do Mar" }]);
const tx = optimisticUpdate("album", "album_1", { title: "Canção do Mar (Remastered)" });
// Call tx.commit() on server 200 OK, or tx.rollback() on network failure:
tx.commit();

console.log("Slug:", slugify("Canção do Mar!")); // "cancao-do-mar"
```

---

## 4. JavaScript / TypeScript API Reference

### `HyleSource` Class & Dataset Operations
- `new HyleSource(options?: string | { storageDir?: string; wasm?: boolean })`
- `.registerDataset(def: HyleDatasetDef | string): boolean` (standalone: `registerDataset`)
- `.putItem(datasetId, itemId, record): boolean` / `.updateItem(datasetId, itemId, record): boolean` (standalone: `putItem`, `updateItem`)
- `.deleteItem(datasetId, itemId): boolean` / `.itemExists(datasetId, itemId): boolean` (standalone: `deleteItem`, `itemExists`)
- `.getField(datasetId, itemId, fieldName): string | null` (standalone: `getField`)
- `.query(datasetId, queryStr): HyleQueryResult` (standalone: `queryDataset`)

### Query Cache, Entity Store & Optimistic Updates
- `normalizeQuery(queryStr: string): string`: Canonicalizes query parameters so equivalent filter orders share a single cache key.
- `cachePut(datasetId, queryStr, jsonPayload, opts?)` / `cacheHydrate(datasetId, queryStr, payload, opts?)`
- `cacheGet(datasetId, queryStr, opts?)` / `cacheInvalidate(datasetId)` / `cacheClear()` / `cacheCount(datasetId)`
- `entityPut(datasetId, itemId, jsonPayload)` / `entityGet(datasetId, itemId)` / `entityInvalidate(datasetId, itemId?)`
- `optimisticUpdate(datasetId, itemId, patch): { revision: number; commit: () => void; rollback: () => void }`
- `subscribeDataset(datasetId, callback): () => void` / `createHyleStore(datasetId)`
- `slugify(text: string): string` / `getHyleSourceWasm(): WebAssembly.Instance` / `version(): string`

---

## 5. C Development Headers & Native Integration

```makefile
CFLAGS += -I./node_modules/@tty-pt/hyle-source/include
LDLIBS += -lhyle-source
```

---

## License

BSD-2-Clause — Copyright (c) 2026, Paulo Quirino <quirinpa@gmail.com>. See [LICENSE](https://github.com/tty-pt/libhyle-source/blob/main/LICENSE).
