#include <stddef.h>
#include <stdint.h>
#include <ttypt/iso.h>

/*─────────────────────────────────────────────────────────────────────────────*
 *                HOST ALLOCATOR IMPORTS (STELE SUBSTRATE)                     *
 *─────────────────────────────────────────────────────────────────────────────*/

__attribute__((import_module("env"), import_name("malloc")))
extern void *host_malloc(size_t sz);

__attribute__((import_module("env"), import_name("free")))
extern void host_free(void *ptr);

#define malloc host_malloc
#define free host_free

/*─────────────────────────────────────────────────────────────────────────────*
 *                      MINIMAL FREESTANDING UTILITIES                         *
 *─────────────────────────────────────────────────────────────────────────────*/

static size_t wasm_strlen(const char *s) {
    if (!s) return 0;
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static int wasm_strcmp(const char *s1, const char *s2) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
}

static int wasm_strncmp(const char *s1, const char *s2, size_t n) {
    if (!s1 || !s2 || n == 0) return 0;
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
}

static char wasm_tolower(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c + ('a' - 'A'));
    return c;
}

static int wasm_strcasecmp(const char *s1, const char *s2) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    while (*s1 && (wasm_tolower(*s1) == wasm_tolower(*s2))) {
        s1++;
        s2++;
    }
    return (int)((unsigned char)wasm_tolower(*s1) - (unsigned char)wasm_tolower(*s2));
}

static char *wasm_strcasestr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        if (wasm_tolower(*haystack) == wasm_tolower(*needle)) {
            const char *h = haystack;
            const char *n = needle;
            while (*h && *n && wasm_tolower(*h) == wasm_tolower(*n)) {
                h++;
                n++;
            }
            if (!*n) return (char *)haystack;
        }
    }
    return NULL;
}

static void *wasm_memcpy(void *dst, const void *src, size_t n) {
    char *d = (char *)dst;
    const char *s = (const char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

static void *wasm_memset(void *dst, int val, size_t n) {
    char *d = (char *)dst;
    while (n--) *d++ = (char)val;
    return dst;
}

static char *wasm_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = wasm_strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (!copy) return NULL;
    wasm_memcpy(copy, s, len + 1);
    return copy;
}

static const char *wasm_strchr(const char *s, int c) {
    if (!s) return NULL;
    while (*s) {
        if (*s == (char)c) return s;
        s++;
    }
    return (c == 0) ? s : NULL;
}

static const char *wasm_strstr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    if (!*needle) return haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack;
            const char *n = needle;
            while (*h && *n && *h == *n) {
                h++;
                n++;
            }
            if (!*n) return haystack;
        }
    }
    return NULL;
}

static int wasm_atoi(const char *s) {
    if (!s) return 0;
    while (*s == ' ' || *s == '\t') s++;
    int sign = 1;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    int res = 0;
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return sign * res;
}

static void buf_append_str(char *out, size_t out_cap, size_t *w, const char *s) {
    if (!out || !w || !s || *w >= out_cap - 1) return;
    while (*s && *w < out_cap - 1) {
        out[(*w)++] = *s++;
    }
    out[*w] = '\0';
}

static void buf_append_u64(char *out, size_t out_cap, size_t *w, uint64_t v) {
    char tmp[32];
    size_t tlen = 0;
    if (v == 0) {
        tmp[tlen++] = '0';
    } else {
        while (v > 0) {
            tmp[tlen++] = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    for (size_t i = 0; i < tlen && *w < out_cap - 1; i++) {
        out[(*w)++] = tmp[tlen - 1 - i];
    }
    out[*w] = '\0';
}

/*─────────────────────────────────────────────────────────────────────────────*
 *                          QUERY STRING NORMALIZATION                         *
 *─────────────────────────────────────────────────────────────────────────────*/

typedef struct {
    char key[64];
    char val[128];
} wasm_query_kv_t;

ISO_EXPORT(int, hyle_source_wasm_normalize_query, const char *in_query, char *out, size_t out_cap) {
    if (!out || out_cap == 0) return -1;
    out[0] = '\0';
    if (!in_query || !in_query[0]) return 0;

    wasm_query_kv_t pairs[32];
    size_t count = 0;

    const char *p = in_query;
    while (*p && count < 32) {
        size_t klen = 0;
        size_t vlen = 0;

        while (*p && *p != '=' && *p != '&' && klen < sizeof(pairs[0].key) - 1) {
            pairs[count].key[klen++] = *p++;
        }
        pairs[count].key[klen] = '\0';

        if (*p == '=') {
            p++;
            while (*p && *p != '&' && vlen < sizeof(pairs[0].val) - 1) {
                pairs[count].val[vlen++] = *p++;
            }
        }
        pairs[count].val[vlen] = '\0';

        if (klen > 0) count++;
        if (*p == '&') p++;
    }

    /* Sort pairs alphabetically by key */
    if (count > 1) {
        for (size_t i = 0; i < count - 1; i++) {
            size_t best = i;
            for (size_t j = i + 1; j < count; j++) {
                if (wasm_strcmp(pairs[j].key, pairs[best].key) < 0) {
                    best = j;
                }
            }
            if (best != i) {
                wasm_query_kv_t tmp = pairs[i];
                pairs[i] = pairs[best];
                pairs[best] = tmp;
            }
        }
    }

    size_t w = 0;
    for (size_t i = 0; i < count; i++) {
        if (i > 0 && w < out_cap - 1) out[w++] = '&';
        buf_append_str(out, out_cap, &w, pairs[i].key);
        if (pairs[i].val[0]) {
            if (w < out_cap - 1) out[w++] = '=';
            buf_append_str(out, out_cap, &w, pairs[i].val);
        }
    }
    out[w] = '\0';
    return (int)w;
}

/*─────────────────────────────────────────────────────────────────────────────*
 *                          TIER 1: QUERY SLICE CACHE                          *
 *─────────────────────────────────────────────────────────────────────────────*/

#define WASM_CACHE_CAPACITY 64

typedef struct {
    char dataset[64];
    char *query;
    char *json;
    size_t len;
    uint32_t seq;
    uint32_t ttl_ms;
    uint32_t tick_created;
} wasm_cache_entry_t;

static wasm_cache_entry_t g_cache[WASM_CACHE_CAPACITY];
static size_t g_cache_count = 0;
static uint32_t g_cache_seq = 0;
static uint32_t g_current_ticks = 1000;

static void cache_entry_free(wasm_cache_entry_t *e) {
    if (!e) return;
    e->dataset[0] = '\0';
    if (e->query) {
        free(e->query);
        e->query = NULL;
    }
    if (e->json) {
        free(e->json);
        e->json = NULL;
    }
    e->len = 0;
    e->seq = 0;
    e->ttl_ms = 0;
    e->tick_created = 0;
}

ISO_EXPORT(int, hyle_source_wasm_cache_put_ttl, const char *dataset_id, const char *query_str, const char *json, size_t len, uint32_t ttl_ms) {
    size_t i;
    size_t target_idx = (size_t)-1;
    uint32_t oldest_seq = UINT32_MAX;
    size_t oldest_idx = 0;

    if (!dataset_id || !dataset_id[0] || !json) return -1;

    char norm_q[512] = {0};
    if (query_str && query_str[0]) {
        hyle_source_wasm_normalize_query(query_str, norm_q, sizeof(norm_q));
    }

    if (len == 0 && json[0]) len = wasm_strlen(json);

    for (i = 0; i < WASM_CACHE_CAPACITY; i++) {
        if (g_cache[i].dataset[0] &&
            wasm_strcmp(g_cache[i].dataset, dataset_id) == 0 &&
            g_cache[i].query &&
            wasm_strcmp(g_cache[i].query, norm_q) == 0) {
            target_idx = i;
            break;
        }
    }

    if (target_idx == (size_t)-1) {
        for (i = 0; i < WASM_CACHE_CAPACITY; i++) {
            if (!g_cache[i].dataset[0]) {
                target_idx = i;
                g_cache_count++;
                break;
            }
            if (g_cache[i].seq < oldest_seq) {
                oldest_seq = g_cache[i].seq;
                oldest_idx = i;
            }
        }
        if (target_idx == (size_t)-1) {
            target_idx = oldest_idx;
            cache_entry_free(&g_cache[target_idx]);
        }
    }

    wasm_cache_entry_t *e = &g_cache[target_idx];
    char *new_query = wasm_strdup(norm_q);
    char *new_json = (char *)malloc(len + 1);
    if (!new_query || !new_json) {
        if (new_query) free(new_query);
        if (new_json) free(new_json);
        return -1;
    }
    wasm_memcpy(new_json, json, len);
    new_json[len] = '\0';

    if (e->query) free(e->query);
    if (e->json) free(e->json);

    size_t dlen = wasm_strlen(dataset_id);
    if (dlen >= sizeof(e->dataset)) dlen = sizeof(e->dataset) - 1;
    wasm_memcpy(e->dataset, dataset_id, dlen);
    e->dataset[dlen] = '\0';

    e->query = new_query;
    e->json = new_json;
    e->len = len;
    e->seq = ++g_cache_seq;
    e->ttl_ms = ttl_ms;
    e->tick_created = ++g_current_ticks;
    return 0;
}

ISO_EXPORT(int, hyle_source_wasm_cache_put, const char *dataset_id, const char *query_str, const char *json, size_t len) {
    return hyle_source_wasm_cache_put_ttl(dataset_id, query_str, json, len, 0);
}

ISO_EXPORT(int, hyle_source_wasm_cache_hydrate, const char *dataset_id, const char *query_str, const char *json, size_t len, uint32_t ttl_ms);

ISO_EXPORT(int, hyle_source_wasm_cache_get_status, const char *dataset_id, const char *query_str, char *out, size_t out_cap, int *is_stale) {
    if (!dataset_id || !dataset_id[0] || !out || out_cap == 0) return -1;

    char norm_q[512] = {0};
    if (query_str && query_str[0]) {
        hyle_source_wasm_normalize_query(query_str, norm_q, sizeof(norm_q));
    }

    for (size_t i = 0; i < WASM_CACHE_CAPACITY; i++) {
        if (g_cache[i].dataset[0] &&
            wasm_strcmp(g_cache[i].dataset, dataset_id) == 0 &&
            g_cache[i].query &&
            wasm_strcmp(g_cache[i].query, norm_q) == 0) {
            g_cache[i].seq = ++g_cache_seq;
            size_t copy_len = g_cache[i].len < (out_cap - 1) ? g_cache[i].len : (out_cap - 1);
            wasm_memcpy(out, g_cache[i].json, copy_len);
            out[copy_len] = '\0';

            if (is_stale) {
                if (g_cache[i].ttl_ms > 0) {
                    uint32_t age = g_current_ticks - g_cache[i].tick_created;
                    *is_stale = (age > g_cache[i].ttl_ms) ? 1 : 0;
                } else {
                    *is_stale = 0;
                }
            }
            return (int)copy_len;
        }
    }
    return -1;
}

ISO_EXPORT(int, hyle_source_wasm_cache_get, const char *dataset_id, const char *query_str, char *out, size_t out_cap) {
    return hyle_source_wasm_cache_get_status(dataset_id, query_str, out, out_cap, NULL);
}

ISO_EXPORT(void, hyle_source_wasm_cache_invalidate, const char *dataset_id) {
    for (size_t i = 0; i < WASM_CACHE_CAPACITY; i++) {
        if (g_cache[i].dataset[0]) {
            if (!dataset_id || !dataset_id[0] || wasm_strcmp(g_cache[i].dataset, dataset_id) == 0) {
                cache_entry_free(&g_cache[i]);
                if (g_cache_count > 0) g_cache_count--;
            }
        }
    }
}

ISO_EXPORT(void, hyle_source_wasm_cache_clear, void) {
    for (size_t i = 0; i < WASM_CACHE_CAPACITY; i++) {
        cache_entry_free(&g_cache[i]);
    }
    g_cache_count = 0;
    g_cache_seq = 0;
}

ISO_EXPORT(uint32_t, hyle_source_wasm_cache_count, const char *dataset_id) {
    uint32_t cnt = 0;
    for (size_t i = 0; i < WASM_CACHE_CAPACITY; i++) {
        if (g_cache[i].dataset[0]) {
            if (!dataset_id || !dataset_id[0] || wasm_strcmp(g_cache[i].dataset, dataset_id) == 0) {
                cnt++;
            }
        }
    }
    return cnt;
}

/*─────────────────────────────────────────────────────────────────────────────*
 *                         TIER 2: ENTITY RECORD CACHE                         *
 *─────────────────────────────────────────────────────────────────────────────*/

#define WASM_ENTITY_CACHE_CAPACITY 128

typedef struct {
    char dataset[64];
    char item_id[64];
    char *json;
    size_t len;
    uint32_t seq;
} wasm_entity_cache_entry_t;

static wasm_entity_cache_entry_t g_entity_cache[WASM_ENTITY_CACHE_CAPACITY];
static size_t g_entity_cache_count = 0;
static uint32_t g_entity_cache_seq = 0;

static void entity_entry_free(wasm_entity_cache_entry_t *e) {
    if (!e) return;
    e->dataset[0] = '\0';
    e->item_id[0] = '\0';
    if (e->json) {
        free(e->json);
        e->json = NULL;
    }
    e->len = 0;
    e->seq = 0;
}

ISO_EXPORT(int, hyle_source_wasm_entity_put, const char *dataset_id, const char *item_id, const char *json, size_t len) {
    size_t i;
    size_t target_idx = (size_t)-1;
    uint32_t oldest_seq = UINT32_MAX;
    size_t oldest_idx = 0;

    if (!dataset_id || !dataset_id[0] || !item_id || !item_id[0] || !json) return -1;
    if (len == 0 && json[0]) len = wasm_strlen(json);

    for (i = 0; i < WASM_ENTITY_CACHE_CAPACITY; i++) {
        if (g_entity_cache[i].dataset[0] &&
            wasm_strcmp(g_entity_cache[i].dataset, dataset_id) == 0 &&
            wasm_strcmp(g_entity_cache[i].item_id, item_id) == 0) {
            target_idx = i;
            break;
        }
    }

    if (target_idx == (size_t)-1) {
        for (i = 0; i < WASM_ENTITY_CACHE_CAPACITY; i++) {
            if (!g_entity_cache[i].dataset[0]) {
                target_idx = i;
                g_entity_cache_count++;
                break;
            }
            if (g_entity_cache[i].seq < oldest_seq) {
                oldest_seq = g_entity_cache[i].seq;
                oldest_idx = i;
            }
        }
        if (target_idx == (size_t)-1) {
            target_idx = oldest_idx;
            entity_entry_free(&g_entity_cache[target_idx]);
        }
    }

    wasm_entity_cache_entry_t *e = &g_entity_cache[target_idx];
    char *new_json = (char *)malloc(len + 1);
    if (!new_json) return -1;
    wasm_memcpy(new_json, json, len);
    new_json[len] = '\0';

    if (e->json) free(e->json);

    size_t dlen = wasm_strlen(dataset_id);
    if (dlen >= sizeof(e->dataset)) dlen = sizeof(e->dataset) - 1;
    wasm_memcpy(e->dataset, dataset_id, dlen);
    e->dataset[dlen] = '\0';

    size_t ilen = wasm_strlen(item_id);
    if (ilen >= sizeof(e->item_id)) ilen = sizeof(e->item_id) - 1;
    wasm_memcpy(e->item_id, item_id, ilen);
    e->item_id[ilen] = '\0';

    e->json = new_json;
    e->len = len;
    e->seq = ++g_entity_cache_seq;
    return 0;
}

static void wasm_hydrate_entities_from_json(const char *dataset_id, const char *json, size_t len)
{
    if (!dataset_id || !dataset_id[0] || !json || len == 0)
        return;

    const char *arr_start = NULL;
    const char *items_key = wasm_strstr(json, "\"items\"");
    if (items_key) {
        arr_start = wasm_strchr(items_key, '[');
    }
    if (!arr_start) {
        arr_start = wasm_strchr(json, '[');
    }
    if (!arr_start)
        return;

    const char *p = arr_start + 1;
    const char *end = json + len;

    while (p < end) {
        while (p < end && *p != '{' && *p != ']')
            p++;
        if (p >= end || *p == ']')
            break;

        const char *obj_start = p;
        int depth = 0;
        int in_str = 0;
        const char *obj_end = NULL;

        while (p < end) {
            if (*p == '\\' && in_str && (p + 1 < end)) {
                p += 2;
                continue;
            }
            if (*p == '"') {
                in_str = !in_str;
            } else if (!in_str) {
                if (*p == '{') {
                    depth++;
                } else if (*p == '}') {
                    depth--;
                    if (depth == 0) {
                        obj_end = p + 1;
                        p++;
                        break;
                    }
                }
            }
            p++;
        }

        if (!obj_end)
            break;

        size_t obj_len = (size_t)(obj_end - obj_start);

        /* Extract "id" field inside obj_start .. obj_end */
        char item_id[64] = {0};
        const char *id_key = obj_start;
        while (id_key < obj_end) {
            const char *cand = wasm_strstr(id_key, "\"id\"");
            if (!cand || cand >= obj_end)
                break;

            if (cand == obj_start || cand[-1] == '{' || cand[-1] == ',' || cand[-1] <= ' ') {
                const char *colon = cand + 4;
                while (colon < obj_end && (*colon == ' ' || *colon == '\t' || *colon == '\r' || *colon == '\n'))
                    colon++;
                if (colon < obj_end && *colon == ':') {
                    const char *val_start = colon + 1;
                    while (val_start < obj_end && (*val_start == ' ' || *val_start == '\t' || *val_start == '\r' || *val_start == '\n'))
                        val_start++;

                    if (val_start < obj_end) {
                        if (*val_start == '"') {
                            val_start++;
                            const char *val_end = val_start;
                            while (val_end < obj_end && *val_end != '"') {
                                if (*val_end == '\\' && val_end + 1 < obj_end)
                                    val_end += 2;
                                else
                                    val_end++;
                            }
                            size_t id_len = (size_t)(val_end - val_start);
                            if (id_len > 0 && id_len < sizeof(item_id)) {
                                wasm_memcpy(item_id, val_start, id_len);
                                item_id[id_len] = '\0';
                            }
                        } else {
                            const char *val_end = val_start;
                            while (val_end < obj_end && *val_end != ',' && *val_end != '}' && *val_end > ' ')
                                val_end++;
                            size_t id_len = (size_t)(val_end - val_start);
                            if (id_len > 0 && id_len < sizeof(item_id)) {
                                wasm_memcpy(item_id, val_start, id_len);
                                item_id[id_len] = '\0';
                            }
                        }
                        break;
                    }
                }
            }
            id_key = cand + 4;
        }

        if (item_id[0]) {
            hyle_source_wasm_entity_put(dataset_id, item_id, obj_start, obj_len);
        }
    }
}

ISO_EXPORT(int, hyle_source_wasm_cache_hydrate, const char *dataset_id, const char *query_str, const char *json, size_t len, uint32_t ttl_ms) {
    if (!dataset_id || !dataset_id[0] || !json) return -1;
    if (len == 0) len = wasm_strlen(json);

    /* 1. Prime Tier 2 Entity Cache by parsing items */
    wasm_hydrate_entities_from_json(dataset_id, json, len);

    /* 2. Prime Tier 1 Query Slice Cache */
    return hyle_source_wasm_cache_put_ttl(dataset_id, query_str, json, len, ttl_ms);
}

ISO_EXPORT(int, hyle_source_wasm_entity_get, const char *dataset_id, const char *item_id, char *out, size_t out_cap) {
    if (!dataset_id || !dataset_id[0] || !item_id || !item_id[0] || !out || out_cap == 0) return -1;

    for (size_t i = 0; i < WASM_ENTITY_CACHE_CAPACITY; i++) {
        if (g_entity_cache[i].dataset[0] &&
            wasm_strcmp(g_entity_cache[i].dataset, dataset_id) == 0 &&
            wasm_strcmp(g_entity_cache[i].item_id, item_id) == 0) {
            g_entity_cache[i].seq = ++g_entity_cache_seq;
            size_t copy_len = g_entity_cache[i].len < (out_cap - 1) ? g_entity_cache[i].len : (out_cap - 1);
            wasm_memcpy(out, g_entity_cache[i].json, copy_len);
            out[copy_len] = '\0';
            return (int)copy_len;
        }
    }
    return -1;
}

ISO_EXPORT(void, hyle_source_wasm_entity_invalidate, const char *dataset_id, const char *item_id) {
    for (size_t i = 0; i < WASM_ENTITY_CACHE_CAPACITY; i++) {
        if (g_entity_cache[i].dataset[0]) {
            if ((!dataset_id || !dataset_id[0] || wasm_strcmp(g_entity_cache[i].dataset, dataset_id) == 0) &&
                (!item_id || !item_id[0] || wasm_strcmp(g_entity_cache[i].item_id, item_id) == 0)) {
                entity_entry_free(&g_entity_cache[i]);
                if (g_entity_cache_count > 0) g_entity_cache_count--;
            }
        }
    }
}

ISO_EXPORT(uint32_t, hyle_source_wasm_entity_count, const char *dataset_id) {
    uint32_t cnt = 0;
    for (size_t i = 0; i < WASM_ENTITY_CACHE_CAPACITY; i++) {
        if (g_entity_cache[i].dataset[0]) {
            if (!dataset_id || !dataset_id[0] || wasm_strcmp(g_entity_cache[i].dataset, dataset_id) == 0) {
                cnt++;
            }
        }
    }
    return cnt;
}

/*─────────────────────────────────────────────────────────────────────────────*
 *                     REACTIVE SUBSCRIPTION CHANGE BUS                        *
 *─────────────────────────────────────────────────────────────────────────────*/

#define WASM_MAX_SUBSCRIBERS 16
#define WASM_EVENT_QUEUE_SIZE 32

typedef struct {
    int id;
    char dataset[64];
    int active;
} wasm_sub_t;

typedef struct {
    char dataset[64];
    char item_id[64];
    char action[16]; /* "put", "update", "delete" */
} wasm_change_event_t;

static wasm_sub_t g_subs[WASM_MAX_SUBSCRIBERS];
static int g_sub_next_id = 1;

static wasm_change_event_t g_events[WASM_EVENT_QUEUE_SIZE];
static size_t g_event_head = 0;
static size_t g_event_count = 0;

static void queue_change_event(const char *dataset_id, const char *item_id, const char *action) {
    /* 1. Invalidate query cache for dataset */
    hyle_source_wasm_cache_invalidate(dataset_id);

    /* 2. Invalidate entity cache on delete */
    if (wasm_strcmp(action, "delete") == 0 && item_id && item_id[0]) {
        hyle_source_wasm_entity_invalidate(dataset_id, item_id);
    }

    /* 3. Record event in ring buffer */
    size_t idx = (g_event_head + g_event_count) % WASM_EVENT_QUEUE_SIZE;
    if (g_event_count >= WASM_EVENT_QUEUE_SIZE) {
        g_event_head = (g_event_head + 1) % WASM_EVENT_QUEUE_SIZE;
    } else {
        g_event_count++;
    }

    wasm_change_event_t *ev = &g_events[idx];
    size_t dlen = wasm_strlen(dataset_id);
    if (dlen >= sizeof(ev->dataset)) dlen = sizeof(ev->dataset) - 1;
    wasm_memcpy(ev->dataset, dataset_id, dlen);
    ev->dataset[dlen] = '\0';

    size_t ilen = item_id ? wasm_strlen(item_id) : 0;
    if (ilen >= sizeof(ev->item_id)) ilen = sizeof(ev->item_id) - 1;
    if (ilen > 0) wasm_memcpy(ev->item_id, item_id, ilen);
    ev->item_id[ilen] = '\0';

    size_t alen = wasm_strlen(action);
    if (alen >= sizeof(ev->action)) alen = sizeof(ev->action) - 1;
    wasm_memcpy(ev->action, action, alen);
    ev->action[alen] = '\0';
}

ISO_EXPORT(int, hyle_source_wasm_optimistic_update, const char *dataset_id, const char *item_id, const char *patch_json, size_t len, char *rollback_out, size_t rollback_cap) {
    if (!dataset_id || !dataset_id[0] || !item_id || !item_id[0] || !patch_json) return -1;
    if (len == 0) len = wasm_strlen(patch_json);

    if (rollback_out && rollback_cap > 0) {
        int copied = hyle_source_wasm_entity_get(dataset_id, item_id, rollback_out, rollback_cap);
        if (copied < 0) {
            rollback_out[0] = '\0';
        }
    }

    int rc = hyle_source_wasm_entity_put(dataset_id, item_id, patch_json, len);
    if (rc != 0) return rc;

    queue_change_event(dataset_id, item_id, "update");
    return 0;
}

ISO_EXPORT(int, hyle_source_wasm_optimistic_rollback, const char *dataset_id, const char *item_id, const char *rollback_snapshot, size_t len) {
    if (!dataset_id || !dataset_id[0] || !item_id || !item_id[0]) return -1;

    if (rollback_snapshot && rollback_snapshot[0]) {
        if (len == 0) len = wasm_strlen(rollback_snapshot);
        hyle_source_wasm_entity_put(dataset_id, item_id, rollback_snapshot, len);
    } else {
        hyle_source_wasm_entity_invalidate(dataset_id, item_id);
    }

    queue_change_event(dataset_id, item_id, "update");
    return 0;
}

ISO_EXPORT(int, hyle_source_wasm_subscribe, const char *dataset_id) {
    for (size_t i = 0; i < WASM_MAX_SUBSCRIBERS; i++) {
        if (!g_subs[i].active) {
            g_subs[i].id = g_sub_next_id++;
            if (dataset_id && dataset_id[0]) {
                size_t dlen = wasm_strlen(dataset_id);
                if (dlen >= sizeof(g_subs[i].dataset)) dlen = sizeof(g_subs[i].dataset) - 1;
                wasm_memcpy(g_subs[i].dataset, dataset_id, dlen);
                g_subs[i].dataset[dlen] = '\0';
            } else {
                g_subs[i].dataset[0] = '\0';
            }
            g_subs[i].active = 1;
            return g_subs[i].id;
        }
    }
    return -1;
}

ISO_EXPORT(int, hyle_source_wasm_unsubscribe, int sub_id) {
    if (sub_id <= 0) return -1;
    for (size_t i = 0; i < WASM_MAX_SUBSCRIBERS; i++) {
        if (g_subs[i].active && g_subs[i].id == sub_id) {
            g_subs[i].active = 0;
            g_subs[i].dataset[0] = '\0';
            return 0;
        }
    }
    return -1;
}

ISO_EXPORT(int, hyle_source_wasm_poll_changes, int sub_id, char *out, size_t out_cap) {
    if (!out || out_cap == 0) return -1;
    out[0] = '\0';

    wasm_sub_t *sub = NULL;
    for (size_t i = 0; i < WASM_MAX_SUBSCRIBERS; i++) {
        if (g_subs[i].active && g_subs[i].id == sub_id) {
            sub = &g_subs[i];
            break;
        }
    }
    if (!sub) return -1;

    size_t w = 0;
    buf_append_str(out, out_cap, &w, "[");
    size_t emitted = 0;

    for (size_t i = 0; i < g_event_count; i++) {
        size_t idx = (g_event_head + i) % WASM_EVENT_QUEUE_SIZE;
        wasm_change_event_t *ev = &g_events[idx];

        if (sub->dataset[0] == '\0' || wasm_strcmp(sub->dataset, ev->dataset) == 0) {
            if (emitted > 0) buf_append_str(out, out_cap, &w, ",");
            buf_append_str(out, out_cap, &w, "{\"dataset\":\"");
            buf_append_str(out, out_cap, &w, ev->dataset);
            buf_append_str(out, out_cap, &w, "\",\"item\":\"");
            buf_append_str(out, out_cap, &w, ev->item_id);
            buf_append_str(out, out_cap, &w, "\",\"action\":\"");
            buf_append_str(out, out_cap, &w, ev->action);
            buf_append_str(out, out_cap, &w, "\"}");
            emitted++;
        }
    }
    buf_append_str(out, out_cap, &w, "]");
    return (int)w;
}

/*─────────────────────────────────────────────────────────────────────────────*
 *                    IN-MEMORY DATASETS & ITEM STORAGE                        *
 *─────────────────────────────────────────────────────────────────────────────*/

#define MAX_DATASETS 16
#define MAX_ID_LEN 64

typedef struct {
    char id[MAX_ID_LEN];
    char *json;
} wasm_item_t;

typedef struct {
    char id[MAX_ID_LEN];
    wasm_item_t *items;
    size_t count;
    size_t cap;
} wasm_dataset_t;

static wasm_dataset_t g_datasets[MAX_DATASETS];
static size_t g_dataset_count = 0;

static wasm_dataset_t *find_dataset(const char *dataset_id) {
    if (!dataset_id || !dataset_id[0]) return NULL;
    for (size_t i = 0; i < g_dataset_count; i++) {
        if (wasm_strcmp(g_datasets[i].id, dataset_id) == 0) {
            return &g_datasets[i];
        }
    }
    return NULL;
}

ISO_EXPORT(int, hyle_source_wasm_register_dataset, const char *dataset_id) {
    if (!dataset_id || !dataset_id[0]) return 0;
    wasm_dataset_t *d = find_dataset(dataset_id);
    if (d) return 1;
    if (g_dataset_count >= MAX_DATASETS) return 0;

    d = &g_datasets[g_dataset_count++];
    size_t dlen = wasm_strlen(dataset_id);
    if (dlen >= sizeof(d->id)) dlen = sizeof(d->id) - 1;
    wasm_memcpy(d->id, dataset_id, dlen);
    d->id[dlen] = '\0';
    d->items = NULL;
    d->count = 0;
    d->cap = 0;
    return 1;
}

ISO_EXPORT(int, hyle_source_wasm_put_item, const char *dataset_id, const char *item_id, const char *json_record) {
    if (!dataset_id || !item_id || !json_record) return 0;
    wasm_dataset_t *d = find_dataset(dataset_id);
    if (!d) {
        if (!hyle_source_wasm_register_dataset(dataset_id)) return 0;
        d = find_dataset(dataset_id);
        if (!d) return 0;
    }

    /* Check if item exists */
    for (size_t i = 0; i < d->count; i++) {
        if (wasm_strcmp(d->items[i].id, item_id) == 0) {
            char *new_json = wasm_strdup(json_record);
            if (!new_json) return 0;
            if (d->items[i].json) free(d->items[i].json);
            d->items[i].json = new_json;
            queue_change_event(dataset_id, item_id, "update");
            return 1;
        }
    }

    /* Append item: expand capacity if needed */
    if (d->count >= d->cap) {
        size_t new_cap = d->cap == 0 ? 16 : d->cap * 2;
        wasm_item_t *new_items = (wasm_item_t *)malloc(new_cap * sizeof(wasm_item_t));
        if (!new_items) return 0;
        if (d->items && d->count > 0) {
            wasm_memcpy(new_items, d->items, d->count * sizeof(wasm_item_t));
            free(d->items);
        }
        d->items = new_items;
        d->cap = new_cap;
    }

    char *new_json = wasm_strdup(json_record);
    if (!new_json) return 0;

    wasm_item_t *it = &d->items[d->count++];
    size_t ilen = wasm_strlen(item_id);
    if (ilen >= sizeof(it->id)) ilen = sizeof(it->id) - 1;
    wasm_memcpy(it->id, item_id, ilen);
    it->id[ilen] = '\0';
    it->json = new_json;
    queue_change_event(dataset_id, item_id, "put");
    return 1;
}

ISO_EXPORT(int, hyle_source_wasm_delete_item, const char *dataset_id, const char *item_id) {
    if (!dataset_id || !item_id) return 0;
    wasm_dataset_t *d = find_dataset(dataset_id);
    if (!d || d->count == 0) return 0;

    for (size_t i = 0; i < d->count; i++) {
        if (wasm_strcmp(d->items[i].id, item_id) == 0) {
            if (d->items[i].json) free(d->items[i].json);
            for (size_t j = i + 1; j < d->count; j++) {
                d->items[j - 1] = d->items[j];
            }
            d->count--;
            queue_change_event(dataset_id, item_id, "delete");
            return 1;
        }
    }
    return 0;
}

ISO_EXPORT(int, hyle_source_wasm_item_exists, const char *dataset_id, const char *item_id) {
    if (!dataset_id || !item_id) return 0;
    wasm_dataset_t *d = find_dataset(dataset_id);
    if (!d) return 0;
    for (size_t i = 0; i < d->count; i++) {
        if (wasm_strcmp(d->items[i].id, item_id) == 0) return 1;
    }
    return 0;
}

static int extract_json_field(const char *json, const char *field, char *out, size_t out_cap) {
    if (!json || !field || !out || out_cap == 0) return 0;
    out[0] = '\0';

    size_t flen = wasm_strlen(field);
    const char *p = json;
    while (*p) {
        if (*p == '"') {
            p++;
            if (wasm_strncmp(p, field, flen) == 0 && p[flen] == '"') {
                p += flen + 1;
                while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':')) p++;
                if (*p == '"') {
                    p++;
                    size_t w = 0;
                    while (*p && *p != '"' && w < out_cap - 1) {
                        if (*p == '\\' && *(p + 1)) p++;
                        out[w++] = *p++;
                    }
                    out[w] = '\0';
                    return 1;
                } else if (*p && *p != '{' && *p != '[') {
                    size_t w = 0;
                    while (*p && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\n' && *p != '\r' && w < out_cap - 1) {
                        out[w++] = *p++;
                    }
                    out[w] = '\0';
                    return 1;
                }
            }
        }
        p++;
    }
    return 0;
}

ISO_EXPORT(int, hyle_source_wasm_get_field, const char *dataset_id, const char *item_id, const char *field_name, char *out, size_t out_cap) {
    if (!dataset_id || !item_id || !field_name || !out || out_cap == 0) return 0;
    out[0] = '\0';
    wasm_dataset_t *d = find_dataset(dataset_id);
    if (!d) return 0;

    for (size_t i = 0; i < d->count; i++) {
        if (wasm_strcmp(d->items[i].id, item_id) == 0) {
            return extract_json_field(d->items[i].json, field_name, out, out_cap);
        }
    }
    return 0;
}

/*─────────────────────────────────────────────────────────────────────────────*
 *                      QUERY PARSING & DATASET FILTERING                      *
 *─────────────────────────────────────────────────────────────────────────────*/

typedef struct {
    char key[64];
    char val[128];
} query_filter_t;

typedef struct {
    char q[128];
    char sort[64];
    int desc;
    int page;
    int limit;
    query_filter_t filters[16];
    size_t filter_count;
} parsed_query_t;

static void parse_query_str(const char *qs, parsed_query_t *pq) {
    wasm_memset(pq, 0, sizeof(*pq));
    pq->page = 1;
    pq->limit = 50;

    if (!qs || !qs[0]) return;

    const char *p = qs;
    while (*p) {
        char key[64] = {0};
        char val[128] = {0};
        size_t klen = 0;
        size_t vlen = 0;

        while (*p && *p != '=' && *p != '&' && klen < sizeof(key) - 1) {
            key[klen++] = *p++;
        }
        key[klen] = '\0';

        if (*p == '=') {
            p++;
            while (*p && *p != '&' && vlen < sizeof(val) - 1) {
                val[vlen++] = *p++;
            }
            val[vlen] = '\0';
        }

        if (klen > 0) {
            if (wasm_strcmp(key, "q") == 0) {
                size_t cpy = vlen < sizeof(pq->q) - 1 ? vlen : sizeof(pq->q) - 1;
                wasm_memcpy(pq->q, val, cpy);
                pq->q[cpy] = '\0';
            } else if (wasm_strcmp(key, "sort") == 0) {
                if (val[0] == '-') {
                    pq->desc = 1;
                    size_t cpy = (vlen - 1) < sizeof(pq->sort) - 1 ? (vlen - 1) : sizeof(pq->sort) - 1;
                    wasm_memcpy(pq->sort, val + 1, cpy);
                    pq->sort[cpy] = '\0';
                } else {
                    size_t cpy = vlen < sizeof(pq->sort) - 1 ? vlen : sizeof(pq->sort) - 1;
                    wasm_memcpy(pq->sort, val, cpy);
                    pq->sort[cpy] = '\0';
                }
            } else if (wasm_strcmp(key, "order") == 0) {
                if (wasm_strcmp(val, "desc") == 0) pq->desc = 1;
            } else if (wasm_strcmp(key, "page") == 0) {
                int pg = wasm_atoi(val);
                if (pg > 0) pq->page = pg;
            } else if (wasm_strcmp(key, "limit") == 0) {
                int lim = wasm_atoi(val);
                if (lim >= 0) pq->limit = lim;
            } else if (pq->filter_count < 16) {
                size_t cpy_k = klen < sizeof(pq->filters[0].key) - 1 ? klen : sizeof(pq->filters[0].key) - 1;
                size_t cpy_v = vlen < sizeof(pq->filters[0].val) - 1 ? vlen : sizeof(pq->filters[0].val) - 1;
                wasm_memcpy(pq->filters[pq->filter_count].key, key, cpy_k);
                pq->filters[pq->filter_count].key[cpy_k] = '\0';
                wasm_memcpy(pq->filters[pq->filter_count].val, val, cpy_v);
                pq->filters[pq->filter_count].val[cpy_v] = '\0';
                pq->filter_count++;
            }
        }

        if (*p == '&') p++;
    }
}

typedef struct {
    const wasm_item_t *item;
    char sort_val[128];
} match_entry_t;

static void sort_matches(match_entry_t *matches, size_t count, int desc) {
    if (count <= 1) return;
    for (size_t i = 0; i < count - 1; i++) {
        size_t best = i;
        for (size_t j = i + 1; j < count; j++) {
            int cmp = wasm_strcmp(matches[j].sort_val, matches[best].sort_val);
            if (desc ? (cmp > 0) : (cmp < 0)) {
                best = j;
            }
        }
        if (best != i) {
            match_entry_t tmp = matches[i];
            matches[i] = matches[best];
            matches[best] = tmp;
        }
    }
}

ISO_EXPORT(int, hyle_source_wasm_query, const char *dataset_id, const char *query_str, char *out, size_t out_cap) {
    if (!out || out_cap == 0) return -1;
    out[0] = '\0';

    if (!dataset_id || !dataset_id[0]) {
        size_t w = 0;
        buf_append_str(out, out_cap, &w, "{\"total\":0,\"ids\":[]}");
        return (int)w;
    }

    wasm_dataset_t *d = find_dataset(dataset_id);
    if (!d || d->count == 0) {
        size_t w = 0;
        buf_append_str(out, out_cap, &w, "{\"total\":0,\"ids\":[]}");
        return (int)w;
    }

    parsed_query_t pq;
    parse_query_str(query_str, &pq);

    match_entry_t *matches = (match_entry_t *)malloc(d->count * sizeof(match_entry_t));
    if (!matches) {
        size_t w = 0;
        buf_append_str(out, out_cap, &w, "{\"total\":0,\"ids\":[]}");
        return (int)w;
    }
    size_t match_count = 0;

    for (size_t i = 0; i < d->count; i++) {
        const wasm_item_t *it = &d->items[i];
        int keep = 1;

        if (pq.q[0]) {
            if (!wasm_strcasestr(it->id, pq.q) && (!it->json || !wasm_strcasestr(it->json, pq.q))) {
                keep = 0;
            }
        }

        if (keep && pq.filter_count > 0) {
            for (size_t f = 0; f < pq.filter_count; f++) {
                char fval[128] = {0};
                if (!extract_json_field(it->json, pq.filters[f].key, fval, sizeof(fval))) {
                    keep = 0;
                    break;
                }
                if (wasm_strcasecmp(fval, pq.filters[f].val) != 0 && !wasm_strcasestr(fval, pq.filters[f].val)) {
                    keep = 0;
                    break;
                }
            }
        }

        if (keep) {
            matches[match_count].item = it;
            matches[match_count].sort_val[0] = '\0';
            if (pq.sort[0]) {
                extract_json_field(it->json, pq.sort, matches[match_count].sort_val, sizeof(matches[match_count].sort_val));
            } else {
                size_t ilen = wasm_strlen(it->id);
                if (ilen >= sizeof(matches[match_count].sort_val)) ilen = sizeof(matches[match_count].sort_val) - 1;
                wasm_memcpy(matches[match_count].sort_val, it->id, ilen);
                matches[match_count].sort_val[ilen] = '\0';
            }
            match_count++;
        }
    }

    if (match_count > 1 && pq.sort[0]) {
        sort_matches(matches, match_count, pq.desc);
    }

    size_t start = 0;
    if (pq.page > 1 && pq.limit > 0) {
        start = (size_t)(pq.page - 1) * (size_t)pq.limit;
    }
    size_t end = match_count;
    if (pq.limit > 0 && (start + (size_t)pq.limit) < end) {
        end = start + (size_t)pq.limit;
    }
    if (start > match_count) start = match_count;

    size_t w = 0;
    buf_append_str(out, out_cap, &w, "{\"total\":");
    buf_append_u64(out, out_cap, &w, (uint64_t)match_count);
    buf_append_str(out, out_cap, &w, ",\"ids\":[");
    for (size_t i = start; i < end; i++) {
        if (i > start) {
            buf_append_str(out, out_cap, &w, ",");
        }
        buf_append_str(out, out_cap, &w, "\"");
        buf_append_str(out, out_cap, &w, matches[i].item->id);
        buf_append_str(out, out_cap, &w, "\"");
    }
    buf_append_str(out, out_cap, &w, "]}");

    free(matches);
    return (int)w;
}

/*─────────────────────────────────────────────────────────────────────────────*
 *                          UTILITIES & HELPERS                                *
 *─────────────────────────────────────────────────────────────────────────────*/

ISO_EXPORT(int, hyle_source_wasm_slugify, const char *in, char *out, size_t out_sz) {
    if (!in || !out || out_sz == 0) return 0;
    size_t w = 0;
    int prev_hyphen = 0;

    for (const char *p = in; *p && w < out_sz - 1; p++) {
        char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out[w++] = c;
            prev_hyphen = 0;
        } else if (c >= 'A' && c <= 'Z') {
            out[w++] = wasm_tolower(c);
            prev_hyphen = 0;
        } else if (c == ' ' || c == '-' || c == '_' || c == '.') {
            if (!prev_hyphen && w > 0) {
                out[w++] = '-';
                prev_hyphen = 1;
            }
        }
    }
    if (w > 0 && out[w - 1] == '-') w--;
    out[w] = '\0';
    return (int)w;
}

ISO_EXPORT(int, hyle_source_wasm_version, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return 0;
    size_t w = 0;
    buf_append_str(out, out_sz, &w, "1.4.0");
    return (int)w;
}
