#include "hyle-source/hyle_source.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

/*─────────────────────────────────────────────────────────────────────────────*
 *                          QUERY STRING NORMALIZATION                         *
 *─────────────────────────────────────────────────────────────────────────────*/

typedef struct {
	char key[64];
	char val[128];
} query_kv_t;

static int compare_query_kv(const void *a, const void *b)
{
	const query_kv_t *ka = (const query_kv_t *)a;
	const query_kv_t *kb = (const query_kv_t *)b;
	return strcmp(ka->key, kb->key);
}

int hyle_source_normalize_query(const char *in_query, char *out_buf, size_t out_cap)
{
	if (!out_buf || out_cap == 0)
		return -1;
	out_buf[0] = '\0';
	if (!in_query || !in_query[0])
		return 0;

	char tmp[512];
	strncpy(tmp, in_query, sizeof(tmp) - 1);
	tmp[sizeof(tmp) - 1] = '\0';

	query_kv_t pairs[32];
	size_t count = 0;

	char *saveptr = NULL;
	char *token = strtok_r(tmp, "&", &saveptr);
	while (token && count < 32) {
		char *eq = strchr(token, '=');
		if (eq) {
			*eq = '\0';
			strncpy(pairs[count].key, token, sizeof(pairs[count].key) - 1);
			pairs[count].key[sizeof(pairs[count].key) - 1] = '\0';
			strncpy(pairs[count].val, eq + 1, sizeof(pairs[count].val) - 1);
			pairs[count].val[sizeof(pairs[count].val) - 1] = '\0';
		} else {
			strncpy(pairs[count].key, token, sizeof(pairs[count].key) - 1);
			pairs[count].key[sizeof(pairs[count].key) - 1] = '\0';
			pairs[count].val[0] = '\0';
		}
		count++;
		token = strtok_r(NULL, "&", &saveptr);
	}

	if (count > 1) {
		qsort(pairs, count, sizeof(query_kv_t), compare_query_kv);
	}

	size_t w = 0;
	for (size_t i = 0; i < count; i++) {
		if (i > 0 && w < out_cap - 1)
			out_buf[w++] = '&';
		size_t klen = strlen(pairs[i].key);
		if (w + klen < out_cap - 1) {
			memcpy(out_buf + w, pairs[i].key, klen);
			w += klen;
		}
		if (pairs[i].val[0]) {
			if (w < out_cap - 1)
				out_buf[w++] = '=';
			size_t vlen = strlen(pairs[i].val);
			if (w + vlen < out_cap - 1) {
				memcpy(out_buf + w, pairs[i].val, vlen);
				w += vlen;
			}
		}
	}
	out_buf[w] = '\0';
	return (int)w;
}

/*─────────────────────────────────────────────────────────────────────────────*
 *                          TIER 1: QUERY SLICE CACHE                          *
 *─────────────────────────────────────────────────────────────────────────────*/

#define HYLE_CACHE_CAPACITY 64

typedef struct {
	char dataset[64];
	char *query;
	char *json;
	size_t len;
	uint32_t seq;
	uint64_t created_at;
	uint32_t ttl_ms;
} hyle_cache_entry_t;

static hyle_cache_entry_t g_cache[HYLE_CACHE_CAPACITY];
static size_t g_cache_count = 0;
static uint32_t g_cache_seq = 0;

static void entry_free(hyle_cache_entry_t *e)
{
	if (!e)
		return;
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
	e->created_at = 0;
	e->ttl_ms = 0;
}

static uint64_t get_now_ms(void)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
		return (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
	}
	return (uint64_t)time(NULL) * 1000;
}

int hyle_source_cache_put_ttl(
        const char *dataset_id, const char *query_str, const char *json,
        size_t len, uint32_t ttl_ms)
{
	size_t i;
	size_t target_idx = (size_t)-1;
	uint32_t oldest_seq = UINT32_MAX;
	size_t oldest_idx = 0;

	if (!dataset_id || !dataset_id[0] || !json)
		return -1;

	char norm_query[512] = {0};
	if (query_str && query_str[0]) {
		hyle_source_normalize_query(query_str, norm_query, sizeof(norm_query));
	}

	if (len == 0 && json[0])
		len = strlen(json);

	/* Check if entry already exists */
	for (i = 0; i < HYLE_CACHE_CAPACITY; i++) {
		if (g_cache[i].dataset[0] &&
		    strcmp(g_cache[i].dataset, dataset_id) == 0 &&
		    g_cache[i].query &&
		    strcmp(g_cache[i].query, norm_query) == 0)
		{
			target_idx = i;
			break;
		}
	}

	if (target_idx == (size_t)-1) {
		/* Find empty slot or oldest LRU slot */
		for (i = 0; i < HYLE_CACHE_CAPACITY; i++) {
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
			entry_free(&g_cache[target_idx]);
		}
	}

	hyle_cache_entry_t *e = &g_cache[target_idx];
	char *new_query = strdup(norm_query);
	char *new_json = malloc(len + 1);
	if (!new_query || !new_json) {
		if (new_query) free(new_query);
		if (new_json) free(new_json);
		return -1;
	}
	memcpy(new_json, json, len);
	new_json[len] = '\0';

	if (e->query)
		free(e->query);
	if (e->json)
		free(e->json);

	snprintf(e->dataset, sizeof(e->dataset), "%s", dataset_id);
	e->query = new_query;
	e->json = new_json;
	e->len = len;
	e->seq = ++g_cache_seq;
	e->created_at = get_now_ms();
	e->ttl_ms = ttl_ms;
	return 0;
}

int hyle_source_cache_put(
        const char *dataset_id, const char *query_str, const char *json,
        size_t len)
{
	return hyle_source_cache_put_ttl(dataset_id, query_str, json, len, 0);
}

const char *hyle_source_cache_get_status(
        const char *dataset_id, const char *query_str, size_t *len_out, int *is_stale)
{
	size_t i;

	if (!dataset_id || !dataset_id[0])
		return NULL;

	char norm_query[512] = {0};
	if (query_str && query_str[0]) {
		hyle_source_normalize_query(query_str, norm_query, sizeof(norm_query));
	}

	for (i = 0; i < HYLE_CACHE_CAPACITY; i++) {
		if (g_cache[i].dataset[0] &&
		    strcmp(g_cache[i].dataset, dataset_id) == 0 &&
		    g_cache[i].query &&
		    strcmp(g_cache[i].query, norm_query) == 0)
		{
			g_cache[i].seq = ++g_cache_seq;
			if (len_out)
				*len_out = g_cache[i].len;

			if (is_stale) {
				if (g_cache[i].ttl_ms > 0) {
					uint64_t now = get_now_ms();
					uint64_t expire_at = g_cache[i].created_at + (uint64_t)g_cache[i].ttl_ms;
					*is_stale = (now >= expire_at) ? 1 : 0;
				} else {
					*is_stale = 0;
				}
			}
			return g_cache[i].json;
		}
	}
	return NULL;
}

const char *hyle_source_cache_get(
        const char *dataset_id, const char *query_str, size_t *len_out)
{
	return hyle_source_cache_get_status(dataset_id, query_str, len_out, NULL);
}

void hyle_source_cache_invalidate(const char *dataset_id)
{
	size_t i;

	for (i = 0; i < HYLE_CACHE_CAPACITY; i++) {
		if (g_cache[i].dataset[0]) {
			if (!dataset_id || !dataset_id[0] ||
			    strcmp(g_cache[i].dataset, dataset_id) == 0)
			{
				entry_free(&g_cache[i]);
				if (g_cache_count > 0)
					g_cache_count--;
			}
		}
	}
}

void hyle_source_cache_clear(void)
{
	size_t i;

	for (i = 0; i < HYLE_CACHE_CAPACITY; i++)
		entry_free(&g_cache[i]);
	g_cache_count = 0;
	g_cache_seq = 0;
}

size_t hyle_source_cache_count(const char *dataset_id)
{
	size_t i, cnt = 0;

	for (i = 0; i < HYLE_CACHE_CAPACITY; i++) {
		if (g_cache[i].dataset[0]) {
			if (!dataset_id || !dataset_id[0] ||
			    strcmp(g_cache[i].dataset, dataset_id) == 0)
				cnt++;
		}
	}
	return cnt;
}

/*─────────────────────────────────────────────────────────────────────────────*
 *                         TIER 2: ENTITY RECORD CACHE                         *
 *─────────────────────────────────────────────────────────────────────────────*/

#define HYLE_ENTITY_CACHE_CAPACITY 256

typedef struct {
	char dataset[64];
	char item_id[64];
	char *json;
	size_t len;
	uint32_t seq;
} hyle_entity_cache_entry_t;

static hyle_entity_cache_entry_t g_entity_cache[HYLE_ENTITY_CACHE_CAPACITY];
static size_t g_entity_cache_count = 0;
static uint32_t g_entity_cache_seq = 0;

static void entity_entry_free(hyle_entity_cache_entry_t *e)
{
	if (!e)
		return;
	e->dataset[0] = '\0';
	e->item_id[0] = '\0';
	if (e->json) {
		free(e->json);
		e->json = NULL;
	}
	e->len = 0;
	e->seq = 0;
}

int hyle_source_entity_cache_put(
        const char *dataset_id, const char *item_id, const char *json,
        size_t len)
{
	size_t i;
	size_t target_idx = (size_t)-1;
	uint32_t oldest_seq = UINT32_MAX;
	size_t oldest_idx = 0;

	if (!dataset_id || !dataset_id[0] || !item_id || !item_id[0] || !json)
		return -1;
	if (len == 0 && json[0])
		len = strlen(json);

	for (i = 0; i < HYLE_ENTITY_CACHE_CAPACITY; i++) {
		if (g_entity_cache[i].dataset[0] &&
		    strcmp(g_entity_cache[i].dataset, dataset_id) == 0 &&
		    strcmp(g_entity_cache[i].item_id, item_id) == 0)
		{
			target_idx = i;
			break;
		}
	}

	if (target_idx == (size_t)-1) {
		for (i = 0; i < HYLE_ENTITY_CACHE_CAPACITY; i++) {
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

	hyle_entity_cache_entry_t *e = &g_entity_cache[target_idx];
	char *new_json = malloc(len + 1);
	if (!new_json)
		return -1;
	memcpy(new_json, json, len);
	new_json[len] = '\0';

	if (e->json)
		free(e->json);

	snprintf(e->dataset, sizeof(e->dataset), "%s", dataset_id);
	snprintf(e->item_id, sizeof(e->item_id), "%s", item_id);
	e->json = new_json;
	e->len = len;
	e->seq = ++g_entity_cache_seq;
	return 0;
}

static void hydrate_entities_from_json(const char *dataset_id, const char *json, size_t len)
{
	if (!dataset_id || !dataset_id[0] || !json || len == 0)
		return;

	const char *arr_start = NULL;
	const char *items_key = strstr(json, "\"items\"");
	if (items_key) {
		arr_start = strchr(items_key, '[');
	}
	if (!arr_start) {
		arr_start = strchr(json, '[');
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
			const char *cand = strstr(id_key, "\"id\"");
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
								memcpy(item_id, val_start, id_len);
								item_id[id_len] = '\0';
							}
						} else {
							const char *val_end = val_start;
							while (val_end < obj_end && *val_end != ',' && *val_end != '}' && *val_end > ' ')
								val_end++;
							size_t id_len = (size_t)(val_end - val_start);
							if (id_len > 0 && id_len < sizeof(item_id)) {
								memcpy(item_id, val_start, id_len);
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
			hyle_source_entity_cache_put(dataset_id, item_id, obj_start, obj_len);
		}
	}
}

int hyle_source_cache_hydrate(
        const char *dataset_id, const char *query_str, const char *json,
        size_t len, uint32_t ttl_ms)
{
	if (!dataset_id || !dataset_id[0] || !json)
		return -1;
	if (len == 0)
		len = strlen(json);

	/* 1. Prime Tier 2 Entity Cache by parsing items */
	hydrate_entities_from_json(dataset_id, json, len);

	/* 2. Prime Tier 1 Query Slice Cache */
	return hyle_source_cache_put_ttl(dataset_id, query_str, json, len, ttl_ms);
}

const char *hyle_source_entity_cache_get(
        const char *dataset_id, const char *item_id, size_t *len_out)
{
	size_t i;

	if (!dataset_id || !dataset_id[0] || !item_id || !item_id[0])
		return NULL;

	for (i = 0; i < HYLE_ENTITY_CACHE_CAPACITY; i++) {
		if (g_entity_cache[i].dataset[0] &&
		    strcmp(g_entity_cache[i].dataset, dataset_id) == 0 &&
		    strcmp(g_entity_cache[i].item_id, item_id) == 0)
		{
			g_entity_cache[i].seq = ++g_entity_cache_seq;
			if (len_out)
				*len_out = g_entity_cache[i].len;
			return g_entity_cache[i].json;
		}
	}
	return NULL;
}

void hyle_source_entity_cache_invalidate(
        const char *dataset_id, const char *item_id)
{
	size_t i;

	for (i = 0; i < HYLE_ENTITY_CACHE_CAPACITY; i++) {
		if (g_entity_cache[i].dataset[0]) {
			if ((!dataset_id || !dataset_id[0] || strcmp(g_entity_cache[i].dataset, dataset_id) == 0) &&
			    (!item_id || !item_id[0] || strcmp(g_entity_cache[i].item_id, item_id) == 0))
			{
				entity_entry_free(&g_entity_cache[i]);
				if (g_entity_cache_count > 0)
					g_entity_cache_count--;
			}
		}
	}
}

size_t hyle_source_entity_cache_count(const char *dataset_id)
{
	size_t i, cnt = 0;

	for (i = 0; i < HYLE_ENTITY_CACHE_CAPACITY; i++) {
		if (g_entity_cache[i].dataset[0]) {
			if (!dataset_id || !dataset_id[0] ||
			    strcmp(g_entity_cache[i].dataset, dataset_id) == 0)
				cnt++;
		}
	}
	return cnt;
}

/*─────────────────────────────────────────────────────────────────────────────*
 *                     REACTIVE SUBSCRIPTION CHANGE BUS                        *
 *─────────────────────────────────────────────────────────────────────────────*/

#define HYLE_MAX_SUBSCRIBERS 32

typedef struct {
	int id;
	char dataset[64];
	hyle_source_change_cb_t cb;
	void *user_data;
	int active;
} hyle_subscription_t;

static hyle_subscription_t g_subscribers[HYLE_MAX_SUBSCRIBERS];
static int g_sub_next_id = 1;

int hyle_source_subscribe(
        const char *dataset_id,
        hyle_source_change_cb_t cb,
        void *user_data)
{
	if (!cb)
		return -1;

	for (size_t i = 0; i < HYLE_MAX_SUBSCRIBERS; i++) {
		if (!g_subscribers[i].active) {
			g_subscribers[i].id = g_sub_next_id++;
			if (dataset_id && dataset_id[0]) {
				snprintf(g_subscribers[i].dataset, sizeof(g_subscribers[i].dataset), "%s", dataset_id);
			} else {
				g_subscribers[i].dataset[0] = '\0';
			}
			g_subscribers[i].cb = cb;
			g_subscribers[i].user_data = user_data;
			g_subscribers[i].active = 1;
			return g_subscribers[i].id;
		}
	}
	return -1;
}

int hyle_source_unsubscribe(int sub_id)
{
	if (sub_id <= 0)
		return -1;

	for (size_t i = 0; i < HYLE_MAX_SUBSCRIBERS; i++) {
		if (g_subscribers[i].active && g_subscribers[i].id == sub_id) {
			g_subscribers[i].active = 0;
			g_subscribers[i].cb = NULL;
			g_subscribers[i].user_data = NULL;
			g_subscribers[i].dataset[0] = '\0';
			return 0;
		}
	}
	return -1;
}

void hyle_source_notify_change(
        const char *dataset_id,
        const char *item_id,
        hyle_source_change_type_t change_type)
{
	if (!dataset_id || !dataset_id[0])
		return;

	/* 1. Automatically invalidate query slice cache for this dataset */
	hyle_source_cache_invalidate(dataset_id);

	/* 2. If item delete/update, invalidate or update entity cache */
	if (item_id && item_id[0] && change_type == HYLE_SOURCE_CHANGE_DELETE) {
		hyle_source_entity_cache_invalidate(dataset_id, item_id);
	}

	/* 3. Dispatch to all active registered subscribers */
	for (size_t i = 0; i < HYLE_MAX_SUBSCRIBERS; i++) {
		if (g_subscribers[i].active && g_subscribers[i].cb) {
			if (g_subscribers[i].dataset[0] == '\0' ||
			    strcmp(g_subscribers[i].dataset, dataset_id) == 0)
			{
				g_subscribers[i].cb(
				        dataset_id, item_id, change_type,
				        g_subscribers[i].user_data);
			}
		}
	}
}

int hyle_source_optimistic_update(
        const char *dataset_id,
        const char *item_id,
        const char *patch_json,
        size_t len,
        char *rollback_snapshot_out,
        size_t rollback_cap)
{
	if (!dataset_id || !dataset_id[0] || !item_id || !item_id[0] || !patch_json)
		return -1;
	if (len == 0)
		len = strlen(patch_json);

	/* 1. Capture current entity record as rollback snapshot */
	size_t cur_len = 0;
	const char *cur = hyle_source_entity_cache_get(dataset_id, item_id, &cur_len);
	if (cur && cur_len > 0) {
		if (rollback_snapshot_out && rollback_cap > 0) {
			size_t copy_len = cur_len < (rollback_cap - 1) ? cur_len : (rollback_cap - 1);
			memcpy(rollback_snapshot_out, cur, copy_len);
			rollback_snapshot_out[copy_len] = '\0';
		}
	} else {
		if (rollback_snapshot_out && rollback_cap > 0) {
			rollback_snapshot_out[0] = '\0';
		}
	}

	/* 2. Write optimistic patch to entity cache */
	int rc = hyle_source_entity_cache_put(dataset_id, item_id, patch_json, len);
	if (rc != 0)
		return rc;

	/* 3. Invalidate query slice cache for this dataset */
	hyle_source_cache_invalidate(dataset_id);

	/* 4. Immediately notify subscribers */
	hyle_source_notify_change(dataset_id, item_id, HYLE_SOURCE_CHANGE_UPDATE);
	return 0;
}

int hyle_source_optimistic_rollback(
        const char *dataset_id,
        const char *item_id,
        const char *rollback_snapshot,
        size_t len)
{
	if (!dataset_id || !dataset_id[0] || !item_id || !item_id[0])
		return -1;

	if (rollback_snapshot && rollback_snapshot[0]) {
		if (len == 0)
			len = strlen(rollback_snapshot);
		hyle_source_entity_cache_put(dataset_id, item_id, rollback_snapshot, len);
	} else {
		hyle_source_entity_cache_invalidate(dataset_id, item_id);
	}

	hyle_source_cache_invalidate(dataset_id);
	hyle_source_notify_change(dataset_id, item_id, HYLE_SOURCE_CHANGE_UPDATE);
	return 0;
}
