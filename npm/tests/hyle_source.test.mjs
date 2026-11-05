import test from "node:test";
import assert from "node:assert/strict";
import {
  HyleSource,
  isNativeLoaded,
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
  normalizeQuery,
  entityPut,
  entityGet,
  entityInvalidate,
  entityCount,
  subscribeDataset,
  optimisticUpdate,
  createHyleStore,
  slugify,
  version,
} from "../dist/index.js";

test("@tty-pt/hyle-source loads native Node-API bindings", () => {
  assert.equal(isNativeLoaded(), true, "Native addon should be loaded in Node");
});

test("@tty-pt/hyle-source in-memory query cache operates correctly", () => {
  cacheClear();
  assert.equal(cacheCount("songs"), 0);

  // Put slices
  const payload1 = JSON.stringify([{ id: "s1", title: "Song 1" }]);
  const payload2 = JSON.stringify([{ id: "s2", title: "Song 2" }]);

  const rc1 = cachePut("songs", "page=1", payload1);
  assert.equal(rc1, 0, "cachePut should return 0 on success");

  const rc2 = cachePut("songs", "page=2", payload2);
  assert.equal(rc2, 0, "cachePut should return 0 on success");

  assert.equal(cacheCount("songs"), 2);

  // Retrieve slices
  const get1 = cacheGet("songs", "page=1");
  assert.equal(get1, payload1);

  const get2 = cacheGet("songs", "page=2");
  assert.equal(get2, payload2);

  const getMissing = cacheGet("songs", "page=999");
  assert.equal(getMissing, null, "Missing cache key should return null");

  // Invalidate single dataset
  cachePut("albums", "all=1", JSON.stringify([{ id: "a1" }]));
  assert.equal(cacheCount("albums"), 1);

  cacheInvalidate("songs");
  assert.equal(cacheCount("songs"), 0, "songs cache should be cleared after invalidate");
  assert.equal(cacheCount("albums"), 1, "albums cache should remain unaffected");

  // Clear all
  cacheClear();
  assert.equal(cacheCount("albums"), 0);
});

test("@tty-pt/hyle-source dataset definition and CRUD persistence", () => {
  const source = new HyleSource("./var/test_hyle_source");

  // 1. Register test dataset with filesystem persistence
  const registered = source.registerDataset({
    id: "test_members",
    keyField: "id",
    itemsPath: "var/test_hyle_source/test_members",
    fields: [
      { name: "title", type: 0, required: true },
      { name: "email", type: 0 },
      { name: "role", type: 0 }
    ]
  });
  assert.equal(registered, true, "registerDataset should succeed");

  // 2. Put item
  const putResult = source.putItem("test_members", "m1", {
    title: "Alice Smith",
    email: "alice@example.com",
    role: "engineer"
  });
  assert.equal(putResult, true, "putItem should succeed");

  // 3. Check item exists and read fields
  assert.equal(source.itemExists("test_members", "m1"), true, "Item m1 should exist");
  assert.equal(source.getField("test_members", "m1", "title"), "Alice Smith");
  assert.equal(source.getField("test_members", "m1", "email"), "alice@example.com");
  assert.equal(source.getField("test_members", "m1", "role"), "engineer");

  // 4. Update item
  const updateResult = source.updateItem("test_members", "m1", {
    title: "Alice Johnson",
    email: "alice.j@example.com",
    role: "lead"
  });
  assert.equal(updateResult, true, "updateItem should succeed");
  assert.equal(source.getField("test_members", "m1", "title"), "Alice Johnson");
  assert.equal(source.getField("test_members", "m1", "email"), "alice.j@example.com");

  // 5. Query dataset
  const qResult = source.query("test_members", "alice");
  assert.equal(typeof qResult.total, "number");
  assert.ok(Array.isArray(qResult.ids));
  assert.ok(qResult.ids.includes("m1"), "Query for 'alice' should return m1");

  // 6. Delete item
  const deleteResult = source.deleteItem("test_members", "m1");
  assert.equal(deleteResult, true, "deleteItem should succeed");
  assert.equal(source.itemExists("test_members", "m1"), false, "Item m1 should no longer exist");
  assert.equal(source.getField("test_members", "m1", "title"), null);
});

test("@tty-pt/hyle-source WebAssembly universal runtime operations", () => {
  const wasmSource = HyleSource.wasm();
  assert.equal(wasmSource.useWasm, true);

  // 1. In-memory WASM Cache
  wasmSource.cacheClear();
  assert.equal(wasmSource.cacheCount("wasm_songs"), 0);

  const payload = JSON.stringify([{ id: "ws1", title: "WASM Song" }]);
  const putRc = wasmSource.cachePut("wasm_songs", "q=wasm", payload);
  assert.equal(putRc, 0);
  assert.equal(wasmSource.cacheCount("wasm_songs"), 1);

  const getCached = wasmSource.cacheGet("wasm_songs", "q=wasm");
  assert.equal(getCached, payload);

  wasmSource.cacheInvalidate("wasm_songs");
  assert.equal(wasmSource.cacheCount("wasm_songs"), 0);

  // 2. In-memory WASM Dataset CRUD & Querying
  const registered = wasmSource.registerDataset({ id: "client_books" });
  assert.equal(registered, true);

  wasmSource.putItem("client_books", "b1", {
    title: "The Hobbit",
    author: "J.R.R. Tolkien",
    genre: "fantasy",
    year: "1937"
  });

  wasmSource.putItem("client_books", "b2", {
    title: "The Fellowship of the Ring",
    author: "J.R.R. Tolkien",
    genre: "fantasy",
    year: "1954"
  });

  wasmSource.putItem("client_books", "b3", {
    title: "1984",
    author: "George Orwell",
    genre: "dystopia",
    year: "1949"
  });

  assert.equal(wasmSource.itemExists("client_books", "b1"), true);
  assert.equal(wasmSource.itemExists("client_books", "b99"), false);

  assert.equal(wasmSource.getField("client_books", "b1", "title"), "The Hobbit");
  assert.equal(wasmSource.getField("client_books", "b3", "genre"), "dystopia");

  // Query: filter by genre=fantasy
  const fantasyRes = wasmSource.query("client_books", "genre=fantasy");
  assert.equal(fantasyRes.total, 2);
  assert.deepEqual(fantasyRes.ids.sort(), ["b1", "b2"]);

  // Query: text search q=orwell
  const orwellRes = wasmSource.query("client_books", "q=orwell");
  assert.equal(orwellRes.total, 1);
  assert.deepEqual(orwellRes.ids, ["b3"]);

  // Query: sorting by year descending with pagination
  const sortedRes = wasmSource.query("client_books", "sort=year&order=desc&page=1&limit=2");
  assert.equal(sortedRes.total, 3);
  assert.equal(sortedRes.ids.length, 2);
  assert.equal(sortedRes.ids[0], "b2"); // 1954
  assert.equal(sortedRes.ids[1], "b3"); // 1949

  // Delete item
  const delRc = wasmSource.deleteItem("client_books", "b3");
  assert.equal(delRc, true);
  assert.equal(wasmSource.itemExists("client_books", "b3"), false);

  // 3. Utilities: slugify and version
  assert.equal(slugify("My Cool Song / Track #1!"), "my-cool-song-track-1");
  assert.equal(version(), "1.3.1");
});

test("@tty-pt/hyle-source framework-agnostic substrate cache & subscription bus", async () => {
  const source = new HyleSource("./var/test_hyle_cache");

  // 1. Canonical Query Key Normalization
  assert.equal(
    source.normalizeQuery("page=1&sort=title&q=alice"),
    "page=1&q=alice&sort=title"
  );
  assert.equal(
    source.normalizeQuery("sort=title&q=alice&page=1"),
    "page=1&q=alice&sort=title"
  );

  // Permutation cache hit
  const payload = JSON.stringify([{ id: "u1", name: "Alice" }]);
  source.cachePut("users", "sort=title&q=alice&page=1", payload);

  // Fetch using differently ordered query string
  const cached = source.cacheGet("users", "page=1&sort=title&q=alice");
  assert.equal(cached, payload, "Reordered query parameters must hit identical cache entry");

  // 2. Two-Tier Entity Record Cache
  source.entityPut("users", "u1", { id: "u1", name: "Alice", role: "lead" });
  assert.equal(source.entityCount("users"), 1);
  const ent = source.entityGet("users", "u1");
  assert.ok(ent && ent.includes("lead"));
  source.entityInvalidate("users", "u1");
  assert.equal(source.entityCount("users"), 0);

  // 3. Reactive Subscription Bus & Automatic Invalidation
  source.registerDataset({
    id: "sub_test",
    keyField: "id",
    itemsPath: "var/test_hyle_cache/sub_test",
    fields: [{ name: "title", type: 0 }]
  });

  // Pre-seed query cache
  source.cachePut("sub_test", "all=1", JSON.stringify([{ id: "item1" }]));
  assert.equal(source.cacheCount("sub_test"), 1);

  const events = [];
  const unsub = source.subscribe("sub_test", (ds, item, action) => {
    events.push({ ds, item, action });
  });

  // Mutation: putItem on new item must notify subscriber with "put" AND automatically invalidate query cache!
  source.putItem("sub_test", "item1", { title: "Item One" });
  assert.equal(events.length, 1);
  assert.equal(events[0].ds, "sub_test");
  assert.equal(events[0].item, "item1");
  assert.equal(events[0].action, "put", "New item insertion must emit 'put' action");
  assert.equal(source.cacheCount("sub_test"), 0, "Query cache must be invalidated automatically on write");

  // Mutation: updateItem on existing item must notify subscriber with "update"
  source.updateItem("sub_test", "item1", { title: "Item One Updated" });
  assert.equal(events.length, 2);
  assert.equal(events[1].action, "update", "Existing item modification must emit 'update' action");

  // Mutation: deleteItem must notify subscriber with "delete"
  source.deleteItem("sub_test", "item1");
  assert.equal(events.length, 3);
  assert.equal(events[2].action, "delete", "Item deletion must emit 'delete' action");

  unsub();

  // Multi-subscriber lifecycle: concurrent listeners and selective unsubscribe
  const eventsA = [];
  const eventsB = [];
  const unsubA = source.subscribe("sub_test", (ds, item, action) => {
    eventsA.push({ ds, item, action });
  });
  const unsubB = source.subscribe("sub_test", (ds, item, action) => {
    eventsB.push({ ds, item, action });
  });

  source.putItem("sub_test", "multi1", { title: "Multi One" });
  assert.equal(eventsA.length, 1);
  assert.equal(eventsB.length, 1);

  // Unsubscribe A; B must continue receiving events
  unsubA();
  source.updateItem("sub_test", "multi1", { title: "Multi One Mod" });
  assert.equal(eventsA.length, 1, "Unsubscribed listener A must receive zero further notifications");
  assert.equal(eventsB.length, 2, "Active listener B must receive continued notifications");

  // Unsubscribe B
  unsubB();
  source.deleteItem("sub_test", "multi1");
  assert.equal(eventsB.length, 2, "Unsubscribed listener B must receive zero further notifications");

  // 4. React 19 createHyleStore External Store Binding & Snapshot Identity
  const reactStore = source.createStore("sub_test");
  assert.equal(typeof reactStore.subscribe, "function");
  assert.equal(typeof reactStore.getSnapshot, "function");

  // Seed cache for reactStore
  source.cachePut("sub_test", "page=1", JSON.stringify([{ id: "rs1", name: "Alpha" }]));

  // Successive getSnapshot calls on unchanged data MUST return identical referential object equality
  const snap1 = reactStore.getSnapshot("page=1");
  const snap2 = reactStore.getSnapshot("page=1");
  assert.ok(snap1 && Array.isArray(snap1));
  assert.equal(snap1, snap2, "Successive getSnapshot() calls on identical cache entry must return identical object reference");

  // Mutating underlying cache returns a new parsed snapshot reference
  source.cachePut("sub_test", "page=1", JSON.stringify([{ id: "rs1", name: "Beta" }]));
  const snap3 = reactStore.getSnapshot("page=1");
  assert.notEqual(snap1, snap3, "getSnapshot() after cache modification must return new updated reference");
  assert.equal(snap3[0].name, "Beta");

  // 5. TTL & Stale-While-Revalidate (SWR) Policy
  const swrPayload = JSON.stringify([{ id: "swr1", title: "Fresh Swr Data" }]);
  source.cachePut("swr_ds", "page=1", swrPayload, { ttl: 50 }); // 50ms TTL

  // Immediate lookup: fresh (isStale === false)
  const fresh = source.cacheGet("swr_ds", "page=1", { status: true });
  assert.ok(fresh, "Cache hit must return result");
  assert.equal(fresh.json, swrPayload);
  assert.equal(fresh.isStale, false, "Fresh lookup must report isStale === false");

  // Wait 70ms for TTL expiration
  await new Promise((resolve) => setTimeout(resolve, 70));

  // SWR lookup: still returns cached data immediately, but reports isStale === true
  const stale = source.cacheGet("swr_ds", "page=1", { status: true });
  assert.ok(stale, "SWR lookup must still return cached data after expiration");
  assert.equal(stale.json, swrPayload);
  assert.equal(stale.isStale, true, "Expired lookup must report isStale === true");

  // 6. Two-Tier Bulk Hydration (cacheHydrate)
  source.cacheClear();
  source.entityInvalidate("catalog");
  assert.equal(source.cacheCount("catalog"), 0);
  assert.equal(source.entityCount("catalog"), 0);

  const hydrationPayload = {
    total: 3,
    items: [
      { id: "c1", title: "Widget One", price: 10 },
      { id: "c2", title: "Widget Two", price: 20 },
      { id: "c3", title: "Widget Three", price: 30 }
    ]
  };

  // Bulk hydrate query slice and entities in ONE synchronous call
  const hydrateRc = source.cacheHydrate("catalog", "category=tools", hydrationPayload, { ttl: 2000 });
  assert.equal(hydrateRc, 0, "cacheHydrate must return 0 on success");

  // Tier 1 Query Slice check:
  const querySlice = source.cacheGet("catalog", "category=tools");
  assert.ok(querySlice, "Query slice must be primed in Tier 1 cache");
  const parsedSlice = JSON.parse(querySlice);
  assert.equal(parsedSlice.total, 3);
  assert.equal(parsedSlice.items.length, 3);

  // Tier 2 Entity Record check:
  assert.equal(source.entityCount("catalog"), 3, "All 3 items must be primed in Tier 2 entity cache");
  const entity1 = source.entityGet("catalog", "c1");
  assert.ok(entity1, "c1 entity must be in Tier 2 cache");
  assert.ok(entity1.includes("Widget One"));

  const entity2 = source.entityGet("catalog", "c2");
  assert.ok(entity2, "c2 entity must be in Tier 2 cache");
  assert.ok(entity2.includes("Widget Two"));

  // Also verify WASM mode cacheHydrate
  const wasm = HyleSource.wasm();
  wasm.cacheClear();
  wasm.entityInvalidate("wasm_cat");
  const wasmHydrateRc = wasm.cacheHydrate("wasm_cat", "type=gadget", hydrationPayload);
  assert.equal(wasmHydrateRc, 0);
  assert.equal(wasm.entityCount("wasm_cat"), 3);
  assert.ok(wasm.entityGet("wasm_cat", "c3").includes("Widget Three"));

  // 7. Optimistic Mutations & Rollback
  source.entityPut("optimistic_ds", "opt1", { id: "opt1", status: "PENDING" });
  assert.equal(source.entityGet("optimistic_ds", "opt1").includes("PENDING"), true);

  const subscriberEvents = [];
  const optUnsub = source.subscribe("optimistic_ds", (ds, item, action) => {
    subscriberEvents.push({ ds, item, action });
  });

  // Apply optimistic update
  const { rollback } = source.optimisticUpdate("optimistic_ds", "opt1", { status: "ACTIVE" });

  // Entity cache immediately reflects updated status
  const afterOpt = source.entityGet("optimistic_ds", "opt1");
  assert.ok(afterOpt && afterOpt.includes("ACTIVE"), "Optimistic update must immediately update entity cache");
  assert.equal(subscriberEvents.length, 1, "Subscribers must be notified immediately on optimistic update");

  // Rollback on simulated network/backend write failure
  rollback();

  // Entity cache restored to PENDING
  const afterRollback = source.entityGet("optimistic_ds", "opt1");
  assert.ok(afterRollback && afterRollback.includes("PENDING"), "Rollback must restore prior entity snapshot");
  assert.equal(subscriberEvents.length, 2, "Subscribers must be notified on rollback");

  optUnsub();
});


