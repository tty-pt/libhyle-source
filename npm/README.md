# @tty-pt/hyle-source

Universal Node.js and TypeScript bindings for **`libhyle-source`**, the dataset persistence and querying engine for Hyle.

## Features

- **Direct Node-API Bindings**: High-performance bindings compiling `libhyle-source` to `.node` binary addons.
- **Dataset Storage & Retrieval**: Register schemas, insert/update items, read fields, and delete items.
- **Full-Text & Metadata Queries**: Execute lexical and structured queries directly over datasets.
- **In-Memory Query Caching**: High-performance LRU query cache avoiding redundant disk queries.
- **TypeScript Support**: Full TypeScript declarations for seamless Next.js and Node.js integration.

## Installation

```bash
npm install @tty-pt/hyle-source @tty-pt/corm @tty-pt/hyle
```

## Quick Start (Node.js / Next.js)

```typescript
import { HyleSource, cachePut, cacheGet } from "@tty-pt/hyle-source";

const source = new HyleSource("./var/data");

// In-memory caching & optimistic mutation hooks
cachePut("members", "active=1", JSON.stringify([{ id: "m1", name: "Alice" }]));
const cached = cacheGet("members", "active=1");
console.log("Cached slice:", cached);
```

## Runtime & Optimistic Cache API

- **Dual Engine (`hyle_source_node.node` & `dist/hyle-source.wasm`)**: Uses native Node-API bindings in Node.js and falls back to `hyle-source.wasm` on `@tty-pt/stele`'s shared memory substrate in browsers and edge workers.
- **Query Cache & Optimistic Updates**: `cachePut`, `cacheGet`, `cacheInvalidate`, `optimisticUpdate`, `optimisticCommit`, and `optimisticRollback` for zero-latency UI mutations with automatic rollback on failure.

## License

BSD-2-Clause
