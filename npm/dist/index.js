import { loadWasmModule, loadNativeAddon, getSubstrate } from "@tty-pt/stele";

let nativeBinding = loadNativeAddon(import.meta.url, "hyle_source");
let wasmInstance = null;

export const isNativeLoaded = () => nativeBinding !== null;

/**
 * Plug-and-play WebAssembly loader for @tty-pt/hyle-source via Stele substrate
 */
export function getHyleSourceWasm() {
  if (wasmInstance) return wasmInstance;
  wasmInstance = loadWasmModule(new URL("./hyle-source.wasm", import.meta.url));
  return wasmInstance;
}

/* ── Canonical Query Key Normalization ── */

export function normalizeQuery(queryStr, options = {}) {
  if (!queryStr) return "";
  if (!options.wasm && nativeBinding?.normalizeQuery) {
    return nativeBinding.normalizeQuery(queryStr);
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  return substrate.withString(queryStr, (qPtr) => {
    const outCap = 512;
    const outPtr = substrate.malloc(outCap);
    try {
      const written = wasm.exports.hyle_source_wasm_normalize_query(qPtr, outPtr, outCap);
      return substrate.readString(outPtr, written > 0 ? written : 0);
    } finally {
      substrate.free(outPtr);
    }
  });
}

/* ── In-Memory Query Cache API (Tier 1) ── */

export function cachePut(datasetId, queryStr, jsonPayload, options = {}) {
  const ttlMs = options.ttl || 0;
  if (!options.wasm && nativeBinding) {
    if (ttlMs > 0 && nativeBinding.cachePutTtl) {
      return nativeBinding.cachePutTtl(datasetId, queryStr, jsonPayload, ttlMs);
    }
    if (nativeBinding.cachePut) {
      return nativeBinding.cachePut(datasetId, queryStr, jsonPayload);
    }
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  const jsonStr = typeof jsonPayload === "string" ? jsonPayload : JSON.stringify(jsonPayload);
  return substrate.withStrings([datasetId, queryStr || "", jsonStr], (dPtr, qPtr, jPtr) => {
    if (ttlMs > 0 && wasm.exports.hyle_source_wasm_cache_put_ttl) {
      return wasm.exports.hyle_source_wasm_cache_put_ttl(dPtr, qPtr, jPtr, jsonStr.length, ttlMs);
    }
    return wasm.exports.hyle_source_wasm_cache_put(dPtr, qPtr, jPtr, jsonStr.length);
  });
}

export function cacheHydrate(datasetId, queryStr, payload, options = {}) {
  const ttlMs = options.ttl || 0;

  // 1. If payload is an object or array, extract items and prime entity cache (Tier 2)
  let slicePayload = payload;
  let itemsToHydrate = [];

  if (Array.isArray(payload)) {
    itemsToHydrate = payload;
  } else if (payload && typeof payload === "object") {
    if (Array.isArray(payload.items)) {
      itemsToHydrate = payload.items;
    }
  } else if (typeof payload === "string") {
    try {
      const parsed = JSON.parse(payload);
      if (Array.isArray(parsed)) {
        itemsToHydrate = parsed;
      } else if (parsed && typeof parsed === "object" && Array.isArray(parsed.items)) {
        itemsToHydrate = parsed.items;
      }
    } catch {
      // payload is raw string
    }
  }

  for (const item of itemsToHydrate) {
    if (item && typeof item === "object") {
      const id = item.id || item._id;
      if (id) {
        entityPut(datasetId, String(id), item, options);
      }
    }
  }

  // 2. Prime query slice cache (Tier 1)
  const sliceStr = typeof slicePayload === "string" ? slicePayload : JSON.stringify(slicePayload);
  if (!options.wasm && nativeBinding?.cacheHydrate) {
    return nativeBinding.cacheHydrate(datasetId, queryStr, sliceStr, ttlMs);
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  return substrate.withStrings([datasetId, queryStr || "", sliceStr], (dPtr, qPtr, jPtr) => {
    if (wasm.exports.hyle_source_wasm_cache_hydrate) {
      return wasm.exports.hyle_source_wasm_cache_hydrate(dPtr, qPtr, jPtr, sliceStr.length, ttlMs);
    }
    return wasm.exports.hyle_source_wasm_cache_put_ttl(dPtr, qPtr, jPtr, sliceStr.length, ttlMs);
  });
}

export function cacheGet(datasetId, queryStr, options = {}) {
  if (!options.wasm && nativeBinding) {
    if (options.status && nativeBinding.cacheGetStatus) {
      return nativeBinding.cacheGetStatus(datasetId, queryStr);
    }
    if (nativeBinding.cacheGet) {
      return nativeBinding.cacheGet(datasetId, queryStr);
    }
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  return substrate.withStrings([datasetId, queryStr || ""], (dPtr, qPtr) => {
    const outCap = 65536;
    const outPtr = substrate.malloc(outCap);
    try {
      if (options.status && wasm.exports.hyle_source_wasm_cache_get_status) {
        const stalePtr = substrate.malloc(4);
        try {
          const written = wasm.exports.hyle_source_wasm_cache_get_status(dPtr, qPtr, outPtr, outCap, stalePtr);
          if (written < 0) return null;
          const json = substrate.readString(outPtr, written);
          const isStale = new Int32Array(substrate.memory.buffer, stalePtr, 1)[0] === 1;
          return { json, isStale };
        } finally {
          substrate.free(stalePtr);
        }
      }
      const written = wasm.exports.hyle_source_wasm_cache_get(dPtr, qPtr, outPtr, outCap);
      if (written < 0) return null;
      return substrate.readString(outPtr, written);
    } finally {
      substrate.free(outPtr);
    }
  });
}

export function cacheInvalidate(datasetId, options = {}) {
  if (!options.wasm && nativeBinding?.cacheInvalidate) {
    nativeBinding.cacheInvalidate(datasetId);
    return;
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  substrate.withString(datasetId || "", (dPtr) => {
    wasm.exports.hyle_source_wasm_cache_invalidate(dPtr);
  });
}

export function cacheClear(options = {}) {
  if (!options.wasm && nativeBinding?.cacheClear) {
    nativeBinding.cacheClear();
    return;
  }
  const wasm = getHyleSourceWasm();
  wasm.exports.hyle_source_wasm_cache_clear();
}

export function cacheCount(datasetId, options = {}) {
  if (!options.wasm && nativeBinding?.cacheCount) {
    return nativeBinding.cacheCount(datasetId);
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  return substrate.withString(datasetId || "", (dPtr) => {
    return wasm.exports.hyle_source_wasm_cache_count(dPtr);
  });
}

/* ── Two-Tier Entity Record Cache (Tier 2) ── */

export function entityPut(datasetId, itemId, jsonPayload, options = {}) {
  if (!options.wasm && nativeBinding?.entityPut) {
    const jsonStr = typeof jsonPayload === "string" ? jsonPayload : JSON.stringify(jsonPayload);
    return nativeBinding.entityPut(datasetId, itemId, jsonStr);
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  const jsonStr = typeof jsonPayload === "string" ? jsonPayload : JSON.stringify(jsonPayload);
  return substrate.withStrings([datasetId, itemId, jsonStr], (dPtr, iPtr, jPtr) => {
    return wasm.exports.hyle_source_wasm_entity_put(dPtr, iPtr, jPtr, jsonStr.length);
  });
}

export function entityGet(datasetId, itemId, options = {}) {
  if (!options.wasm && nativeBinding?.entityGet) {
    return nativeBinding.entityGet(datasetId, itemId);
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  return substrate.withStrings([datasetId, itemId], (dPtr, iPtr) => {
    const outCap = 16384;
    const outPtr = substrate.malloc(outCap);
    try {
      const written = wasm.exports.hyle_source_wasm_entity_get(dPtr, iPtr, outPtr, outCap);
      if (written < 0) return null;
      return substrate.readString(outPtr, written);
    } finally {
      substrate.free(outPtr);
    }
  });
}

export function entityInvalidate(datasetId, itemId, options = {}) {
  if (!options.wasm && nativeBinding?.entityInvalidate) {
    nativeBinding.entityInvalidate(datasetId, itemId);
    return;
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  substrate.withStrings([datasetId || "", itemId || ""], (dPtr, iPtr) => {
    wasm.exports.hyle_source_wasm_entity_invalidate(dPtr, iPtr);
  });
}

export function entityCount(datasetId, options = {}) {
  if (!options.wasm && nativeBinding?.entityCount) {
    return nativeBinding.entityCount(datasetId);
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  return substrate.withString(datasetId || "", (dPtr) => {
    return wasm.exports.hyle_source_wasm_entity_count(dPtr);
  });
}

/* ── Reactive Subscription Event Bus ── */

const jsSubscribers = new Map();
let jsSubNextId = 1;

export function subscribeDataset(datasetId, callback, options = {}) {
  if (typeof callback !== "function") {
    throw new TypeError("Callback must be a function");
  }

  // 1. Native Node-API subscription
  if (!options.wasm && nativeBinding?.subscribe) {
    const subId = nativeBinding.subscribe(datasetId || "", callback);
    return () => {
      if (nativeBinding?.unsubscribe) {
        nativeBinding.unsubscribe(subId);
      }
    };
  }

  // 2. WebAssembly / In-Memory JS subscription
  const subId = jsSubNextId++;
  const entry = { id: subId, datasetId: datasetId || "", callback };
  jsSubscribers.set(subId, entry);

  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  let wasmSubId = 0;
  if (wasm.exports.hyle_source_wasm_subscribe) {
    substrate.withString(datasetId || "", (dPtr) => {
      wasmSubId = wasm.exports.hyle_source_wasm_subscribe(dPtr);
    });
  }

  return () => {
    jsSubscribers.delete(subId);
    if (wasmSubId > 0 && wasm.exports.hyle_source_wasm_unsubscribe) {
      wasm.exports.hyle_source_wasm_unsubscribe(wasmSubId);
    }
  };
}

export const subscribe = subscribeDataset;

function notifyJsSubscribers(datasetId, itemId, action) {
  for (const entry of jsSubscribers.values()) {
    if (!entry.datasetId || entry.datasetId === datasetId) {
      try {
        entry.callback(datasetId, itemId, action);
      } catch (err) {
        console.error("Subscriber notification error:", err);
      }
    }
  }
}

/* ── Dataset Registration & Item CRUD API ── */

export function registerDataset(def, options = {}) {
  if (!options.wasm && nativeBinding?.registerDataset) {
    return nativeBinding.registerDataset(def);
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  const dsId = typeof def === "string" ? def : def.id;
  return substrate.withString(dsId, (dPtr) => {
    return wasm.exports.hyle_source_wasm_register_dataset(dPtr) === 1;
  });
}

export function updateItem(datasetId, itemId, record, options = {}) {
  if (!options.wasm && nativeBinding?.updateItem) {
    return nativeBinding.updateItem(datasetId, itemId, record);
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  const jsonStr = typeof record === "string" ? record : JSON.stringify(record);
  const ok = substrate.withStrings([datasetId, itemId, jsonStr], (dPtr, iPtr, jPtr) => {
    return wasm.exports.hyle_source_wasm_put_item(dPtr, iPtr, jPtr) === 1;
  });
  if (ok) {
    notifyJsSubscribers(datasetId, itemId, "update");
  }
  return ok;
}

export const putItem = updateItem;

export function optimisticUpdate(datasetId, itemId, patch, options = {}) {
  const prevSnapshot = entityGet(datasetId, itemId, options);

  let merged;
  if (prevSnapshot) {
    try {
      const prevObj = typeof prevSnapshot === "string" ? JSON.parse(prevSnapshot) : prevSnapshot;
      const patchObj = typeof patch === "string" ? JSON.parse(patch) : patch;
      merged = { ...prevObj, ...patchObj };
    } catch {
      merged = patch;
    }
  } else {
    merged = patch;
  }

  const patchStr = typeof merged === "string" ? merged : JSON.stringify(merged);

  if (!options.wasm && nativeBinding?.optimisticUpdate) {
    const rawPrev = nativeBinding.optimisticUpdate(datasetId, itemId, patchStr);
    return {
      rollback: () => {
        if (nativeBinding?.optimisticRollback) {
          nativeBinding.optimisticRollback(datasetId, itemId, rawPrev);
        }
      }
    };
  }

  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  let rollbackRaw = null;

  substrate.withStrings([datasetId, itemId, patchStr], (dPtr, iPtr, pPtr) => {
    const outCap = 65536;
    const outPtr = substrate.malloc(outCap);
    try {
      if (wasm.exports.hyle_source_wasm_optimistic_update) {
        wasm.exports.hyle_source_wasm_optimistic_update(dPtr, iPtr, pPtr, patchStr.length, outPtr, outCap);
        rollbackRaw = substrate.readString(outPtr, 65536);
      }
    } finally {
      substrate.free(outPtr);
    }
  });

  notifyJsSubscribers(datasetId, itemId, "update");

  return {
    rollback: () => {
      substrate.withStrings([datasetId, itemId, rollbackRaw || ""], (dPtr, iPtr, rPtr) => {
        if (wasm.exports.hyle_source_wasm_optimistic_rollback) {
          wasm.exports.hyle_source_wasm_optimistic_rollback(dPtr, iPtr, rPtr, rollbackRaw ? rollbackRaw.length : 0);
        }
      });
      notifyJsSubscribers(datasetId, itemId, "update");
    }
  };
}

export function deleteItem(datasetId, itemId, options = {}) {
  if (!options.wasm && nativeBinding?.deleteItem) {
    return nativeBinding.deleteItem(datasetId, itemId);
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  const ok = substrate.withStrings([datasetId, itemId], (dPtr, iPtr) => {
    return wasm.exports.hyle_source_wasm_delete_item(dPtr, iPtr) === 1;
  });
  if (ok) {
    notifyJsSubscribers(datasetId, itemId, "delete");
  }
  return ok;
}

export function itemExists(datasetId, itemId, options = {}) {
  if (!options.wasm && nativeBinding?.itemExists) {
    return nativeBinding.itemExists(datasetId, itemId);
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  return substrate.withStrings([datasetId, itemId], (dPtr, iPtr) => {
    return wasm.exports.hyle_source_wasm_item_exists(dPtr, iPtr) === 1;
  });
}

export function getField(datasetId, itemId, fieldName, options = {}) {
  if (!options.wasm && nativeBinding?.getField) {
    return nativeBinding.getField(datasetId, itemId, fieldName);
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  return substrate.withStrings([datasetId, itemId, fieldName], (dPtr, iPtr, fPtr) => {
    const outCap = 4096;
    const outPtr = substrate.malloc(outCap);
    try {
      const ok = wasm.exports.hyle_source_wasm_get_field(dPtr, iPtr, fPtr, outPtr, outCap);
      if (!ok) return null;
      return substrate.readString(outPtr);
    } finally {
      substrate.free(outPtr);
    }
  });
}

export function queryDataset(datasetId, queryStr, options = {}) {
  if (!options.wasm && nativeBinding?.queryDataset) {
    return nativeBinding.queryDataset(datasetId, queryStr);
  }
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  return substrate.withStrings([datasetId, queryStr || ""], (dPtr, qPtr) => {
    const outCap = 65536;
    const outPtr = substrate.malloc(outCap);
    try {
      const written = wasm.exports.hyle_source_wasm_query(dPtr, qPtr, outPtr, outCap);
      if (written <= 0) return { total: 0, ids: [] };
      const jsonStr = substrate.readString(outPtr, written);
      return JSON.parse(jsonStr);
    } finally {
      substrate.free(outPtr);
    }
  });
}

export function slugify(text) {
  if (!text) return "";
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  return substrate.withString(text, (inPtr) => {
    const outCap = text.length * 2 + 16;
    const outPtr = substrate.malloc(outCap);
    try {
      const written = wasm.exports.hyle_source_wasm_slugify(inPtr, outPtr, outCap);
      return substrate.readString(outPtr, written);
    } finally {
      substrate.free(outPtr);
    }
  });
}

export function version() {
  const wasm = getHyleSourceWasm();
  const substrate = getSubstrate();
  const outCap = 32;
  const outPtr = substrate.malloc(outCap);
  try {
    const written = wasm.exports.hyle_source_wasm_version(outPtr, outCap);
    return substrate.readString(outPtr, written);
  } finally {
    substrate.free(outPtr);
  }
}

/* ── React 19 useSyncExternalStore Adapter ── */

/**
 * Creates an external store binding compatible with React 19 useSyncExternalStore
 * @param {string} datasetId
 * @param {object} [options]
 */
export function createHyleStore(datasetId, options = {}) {
  const snapshotCache = new Map();

  return {
    subscribe: (callback) => subscribeDataset(datasetId, callback, options),
    getSnapshot: (queryStr = "") => {
      const cached = cacheGet(datasetId, queryStr, options);
      if (cached !== null && cached !== undefined) {
        if (typeof cached === "string") {
          const entry = snapshotCache.get(queryStr);
          if (entry && entry.raw === cached) {
            return entry.parsed;
          }
          const parsed = JSON.parse(cached);
          snapshotCache.set(queryStr, { raw: cached, parsed });
          return parsed;
        }
        return cached;
      }
      return queryDataset(datasetId, queryStr, options);
    }
  };
}

/* ── High-Level HyleSource Class ── */

export class HyleSource {
  constructor(options = {}) {
    if (typeof options === "string") {
      this.storageDir = options;
      this.useWasm = false;
    } else {
      this.storageDir = options.storageDir || "./var/data";
      this.useWasm = Boolean(options.wasm || !nativeBinding);
    }
  }

  static wasm() {
    return new HyleSource({ wasm: true });
  }

  registerDataset(def) {
    const d = { ...def };
    if (!this.useWasm && !d.itemsPath && !d.path) {
      d.itemsPath = `${this.storageDir.replace(/\/+$/, "")}/${def.id}`;
    }
    return registerDataset(d, { wasm: this.useWasm });
  }

  updateItem(datasetId, itemId, record) {
    return updateItem(datasetId, itemId, record, { wasm: this.useWasm });
  }

  putItem(datasetId, itemId, record) {
    return updateItem(datasetId, itemId, record, { wasm: this.useWasm });
  }

  deleteItem(datasetId, itemId) {
    return deleteItem(datasetId, itemId, { wasm: this.useWasm });
  }

  itemExists(datasetId, itemId) {
    return itemExists(datasetId, itemId, { wasm: this.useWasm });
  }

  getField(datasetId, itemId, fieldName) {
    return getField(datasetId, itemId, fieldName, { wasm: this.useWasm });
  }

  query(datasetId, queryStr) {
    return queryDataset(datasetId, queryStr, { wasm: this.useWasm });
  }

  normalizeQuery(queryStr) {
    return normalizeQuery(queryStr, { wasm: this.useWasm });
  }

  cachePut(datasetId, queryStr, jsonPayload, options = {}) {
    return cachePut(datasetId, queryStr, jsonPayload, { wasm: this.useWasm, ...options });
  }

  cacheHydrate(datasetId, queryStr, payload, options = {}) {
    return cacheHydrate(datasetId, queryStr, payload, { wasm: this.useWasm, ...options });
  }

  cacheGet(datasetId, queryStr, options = {}) {
    return cacheGet(datasetId, queryStr, { wasm: this.useWasm, ...options });
  }

  cacheInvalidate(datasetId) {
    cacheInvalidate(datasetId, { wasm: this.useWasm });
  }

  cacheClear() {
    cacheClear({ wasm: this.useWasm });
  }

  cacheCount(datasetId) {
    return cacheCount(datasetId, { wasm: this.useWasm });
  }

  entityPut(datasetId, itemId, jsonPayload) {
    return entityPut(datasetId, itemId, jsonPayload, { wasm: this.useWasm });
  }

  entityGet(datasetId, itemId) {
    return entityGet(datasetId, itemId, { wasm: this.useWasm });
  }

  entityInvalidate(datasetId, itemId) {
    entityInvalidate(datasetId, itemId, { wasm: this.useWasm });
  }

  entityCount(datasetId) {
    return entityCount(datasetId, { wasm: this.useWasm });
  }

  subscribe(datasetId, callback) {
    return subscribeDataset(datasetId, callback, { wasm: this.useWasm });
  }

  optimisticUpdate(datasetId, itemId, patch, options = {}) {
    return optimisticUpdate(datasetId, itemId, patch, { ...options, wasm: this.useWasm });
  }

  createStore(datasetId) {
    return createHyleStore(datasetId, { wasm: this.useWasm });
  }
}

export default {
  HyleSource,
  isNativeLoaded,
  getHyleSourceWasm,
  normalizeQuery,
  registerDataset,
  updateItem,
  putItem,
  deleteItem,
  itemExists,
  getField,
  queryDataset,
  cachePut,
  cacheHydrate,
  cacheGet,
  cacheInvalidate,
  cacheClear,
  cacheCount,
  entityPut,
  entityGet,
  entityInvalidate,
  entityCount,
  subscribeDataset,
  subscribe,
  optimisticUpdate,
  createHyleStore,
  slugify,
  version,
};
