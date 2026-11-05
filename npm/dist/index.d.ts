export interface HyleFieldDef {
  name: string;
  file?: string;
  type?: number;
  writable?: boolean;
  required?: boolean;
  targetSource?: string;
}

export interface HyleDatasetDef {
  id: string;
  keyField?: string;
  itemsPath?: string;
  path?: string;
  volatile?: boolean;
  fields?: HyleFieldDef[];
}

export interface HyleQueryResult {
  total: number;
  ids: string[];
  handle?: number;
}

export interface HyleSourceOptions {
  storageDir?: string;
  wasm?: boolean;
}

export interface HyleCallOpts {
  wasm?: boolean;
}

export interface HyleCachePutOpts extends HyleCallOpts {
  ttl?: number;
}

export interface HyleCacheGetOpts extends HyleCallOpts {
  status?: boolean;
}

export interface HyleCacheStatusResult {
  json: string;
  isStale: boolean;
}

export type HyleChangeAction = "put" | "update" | "delete" | "invalidate";
export type HyleSubscriberCb = (datasetId: string, itemId: string, action: HyleChangeAction) => void;
export type UnsubscribeFn = () => void;

export interface HyleExternalStore {
  subscribe: (callback: () => void) => UnsubscribeFn;
  getSnapshot: (queryStr?: string) => HyleQueryResult | any;
}

export declare function isNativeLoaded(): boolean;
export declare function getHyleSourceWasm(): WebAssembly.Instance;

export declare function normalizeQuery(queryStr: string, options?: HyleCallOpts): string;

export declare function registerDataset(def: HyleDatasetDef | string, options?: HyleCallOpts): boolean;

export declare function updateItem(
  datasetId: string,
  itemId: string,
  record: Record<string, any> | string,
  options?: HyleCallOpts
): boolean;

export declare function putItem(
  datasetId: string,
  itemId: string,
  record: Record<string, any> | string,
  options?: HyleCallOpts
): boolean;

export declare function deleteItem(
  datasetId: string,
  itemId: string,
  options?: HyleCallOpts
): boolean;

export declare function itemExists(
  datasetId: string,
  itemId: string,
  options?: HyleCallOpts
): boolean;

export declare function getField(
  datasetId: string,
  itemId: string,
  fieldName: string,
  options?: HyleCallOpts
): string | null;

export declare function queryDataset(
  datasetId: string,
  queryStr: string,
  options?: HyleCallOpts
): HyleQueryResult;

export declare function cachePut(
  datasetId: string,
  queryStr: string,
  jsonPayload: string | Record<string, any>,
  options?: HyleCachePutOpts
): number;

export declare function cacheHydrate(
  datasetId: string,
  queryStr: string,
  payload: string | Record<string, any> | Array<any>,
  options?: HyleCachePutOpts
): number;

export declare function cacheGet(
  datasetId: string,
  queryStr: string,
  options?: HyleCacheGetOpts
): string | HyleCacheStatusResult | null;

export declare function cacheInvalidate(datasetId: string, options?: HyleCallOpts): void;

export declare function cacheClear(options?: HyleCallOpts): void;

export declare function cacheCount(datasetId: string, options?: HyleCallOpts): number;

export declare function entityPut(
  datasetId: string,
  itemId: string,
  jsonPayload: string | Record<string, any>,
  options?: HyleCallOpts
): number;

export declare function entityGet(
  datasetId: string,
  itemId: string,
  options?: HyleCallOpts
): string | null;

export declare function entityInvalidate(
  datasetId: string,
  itemId?: string,
  options?: HyleCallOpts
): void;

export declare function entityCount(datasetId?: string, options?: HyleCallOpts): number;

export declare function subscribeDataset(
  datasetId: string,
  callback: HyleSubscriberCb,
  options?: HyleCallOpts
): UnsubscribeFn;

export declare const subscribe: typeof subscribeDataset;

export declare function createHyleStore(datasetId: string, options?: HyleCallOpts): HyleExternalStore;

export declare function slugify(text: string): string;

export declare function version(): string;

export declare class HyleSource {
  constructor(options?: string | HyleSourceOptions);
  readonly storageDir: string;
  readonly useWasm: boolean;

  static wasm(): HyleSource;

  registerDataset(def: HyleDatasetDef | string): boolean;
  updateItem(datasetId: string, itemId: string, record: Record<string, any> | string): boolean;
  putItem(datasetId: string, itemId: string, record: Record<string, any> | string): boolean;
  deleteItem(datasetId: string, itemId: string): boolean;
  itemExists(datasetId: string, itemId: string): boolean;
  getField(datasetId: string, itemId: string, fieldName: string): string | null;
  query(datasetId: string, queryStr: string): HyleQueryResult;

  normalizeQuery(queryStr: string): string;
  cachePut(datasetId: string, queryStr: string, jsonPayload: string | Record<string, any>, options?: HyleCachePutOpts): number;
  cacheHydrate(datasetId: string, queryStr: string, payload: string | Record<string, any> | Array<any>, options?: HyleCachePutOpts): number;
  cacheGet(datasetId: string, queryStr: string, options?: HyleCacheGetOpts): string | HyleCacheStatusResult | null;
  cacheInvalidate(datasetId: string): void;
  cacheClear(): void;
  cacheCount(datasetId: string): number;

  entityPut(datasetId: string, itemId: string, jsonPayload: string | Record<string, any>): number;
  entityGet(datasetId: string, itemId: string): string | null;
  entityInvalidate(datasetId: string, itemId?: string): void;
  entityCount(datasetId?: string): number;

  subscribe(datasetId: string, callback: HyleSubscriberCb): UnsubscribeFn;
  createStore(datasetId: string): HyleExternalStore;
}

declare const _default: {
  HyleSource: typeof HyleSource;
  isNativeLoaded: typeof isNativeLoaded;
  getHyleSourceWasm: typeof getHyleSourceWasm;
  normalizeQuery: typeof normalizeQuery;
  registerDataset: typeof registerDataset;
  updateItem: typeof updateItem;
  putItem: typeof putItem;
  deleteItem: typeof deleteItem;
  itemExists: typeof itemExists;
  getField: typeof getField;
  queryDataset: typeof queryDataset;
  cachePut: typeof cachePut;
  cacheHydrate: typeof cacheHydrate;
  cacheGet: typeof cacheGet;
  cacheInvalidate: typeof cacheInvalidate;
  cacheClear: typeof cacheClear;
  cacheCount: typeof cacheCount;
  entityPut: typeof entityPut;
  entityGet: typeof entityGet;
  entityInvalidate: typeof entityInvalidate;
  entityCount: typeof entityCount;
  subscribeDataset: typeof subscribeDataset;
  subscribe: typeof subscribe;
  createHyleStore: typeof createHyleStore;
  slugify: typeof slugify;
  version: typeof version;
};

export default _default;
