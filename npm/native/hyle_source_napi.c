#include <node_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <hyle-source/hyle_source.h>
#include <ttypt/corm.h>

/* ── Query Cache Bindings ── */

static napi_value NapiNormalizeQuery(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_type_error(env, NULL, "Expected (queryString)");
        return NULL;
    }

    char query_str[512] = {0};
    size_t q_len = 0;
    napi_get_value_string_utf8(env, args[0], query_str, sizeof(query_str) - 1, &q_len);

    char norm_buf[512] = {0};
    hyle_source_normalize_query(query_str, norm_buf, sizeof(norm_buf));

    napi_value res;
    napi_create_string_utf8(env, norm_buf, strlen(norm_buf), &res);
    return res;
}

static napi_value NapiCachePutTtl(napi_env env, napi_callback_info info) {
    size_t argc = 4;
    napi_value args[4];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 4) {
        napi_throw_type_error(env, NULL, "Expected (datasetId, queryStr, jsonPayload, ttlMs)");
        return NULL;
    }

    char dataset_id[128] = {0};
    size_t d_len = 0;
    napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);

    char query_str[512] = {0};
    size_t q_len = 0;
    napi_get_value_string_utf8(env, args[1], query_str, sizeof(query_str) - 1, &q_len);

    size_t json_len = 0;
    napi_get_value_string_utf8(env, args[2], NULL, 0, &json_len);
    char *json_buf = (char *)malloc(json_len + 1);
    if (!json_buf) {
        napi_throw_error(env, NULL, "Out of memory");
        return NULL;
    }
    napi_get_value_string_utf8(env, args[2], json_buf, json_len + 1, &json_len);

    uint32_t ttl_ms = 0;
    napi_get_value_uint32(env, args[3], &ttl_ms);

    int rc = hyle_source_cache_put_ttl(dataset_id, query_str, json_buf, json_len, ttl_ms);
    free(json_buf);

    napi_value result;
    napi_create_int32(env, rc, &result);
    return result;
}

static napi_value NapiCacheHydrate(napi_env env, napi_callback_info info) {
    size_t argc = 4;
    napi_value args[4];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_type_error(env, NULL, "Expected (datasetId, queryStr, jsonPayload, [ttlMs])");
        return NULL;
    }

    char dataset_id[128] = {0};
    size_t d_len = 0;
    napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);

    char query_str[512] = {0};
    size_t q_len = 0;
    napi_get_value_string_utf8(env, args[1], query_str, sizeof(query_str) - 1, &q_len);

    size_t json_len = 0;
    napi_get_value_string_utf8(env, args[2], NULL, 0, &json_len);
    char *json_buf = (char *)malloc(json_len + 1);
    if (!json_buf) {
        napi_throw_error(env, NULL, "Out of memory");
        return NULL;
    }
    napi_get_value_string_utf8(env, args[2], json_buf, json_len + 1, &json_len);

    uint32_t ttl_ms = 0;
    if (argc >= 4) {
        napi_get_value_uint32(env, args[3], &ttl_ms);
    }

    int rc = hyle_source_cache_hydrate(dataset_id, query_str, json_buf, json_len, ttl_ms);
    free(json_buf);

    napi_value result;
    napi_create_int32(env, rc, &result);
    return result;
}

static napi_value NapiCacheGetStatus(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_type_error(env, NULL, "Expected (datasetId, queryStr)");
        return NULL;
    }

    char dataset_id[128] = {0};
    size_t d_len = 0;
    napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);

    char query_str[512] = {0};
    size_t q_len = 0;
    napi_get_value_string_utf8(env, args[1], query_str, sizeof(query_str) - 1, &q_len);

    size_t out_len = 0;
    int is_stale = 0;
    const char *cached = hyle_source_cache_get_status(dataset_id, query_str, &out_len, &is_stale);
    if (!cached || out_len == 0) {
        napi_value null_val;
        napi_get_null(env, &null_val);
        return null_val;
    }

    napi_value res_obj;
    napi_create_object(env, &res_obj);

    napi_value json_val;
    napi_create_string_utf8(env, cached, out_len, &json_val);
    napi_set_named_property(env, res_obj, "json", json_val);

    napi_value stale_val;
    napi_get_boolean(env, is_stale ? true : false, &stale_val);
    napi_set_named_property(env, res_obj, "isStale", stale_val);

    return res_obj;
}

/* ── Entity Record Cache Bindings ── */

static napi_value NapiEntityPut(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_type_error(env, NULL, "Expected (datasetId, itemId, jsonPayload)");
        return NULL;
    }

    char dataset_id[128] = {0};
    size_t d_len = 0;
    napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);

    char item_id[128] = {0};
    size_t i_len = 0;
    napi_get_value_string_utf8(env, args[1], item_id, sizeof(item_id) - 1, &i_len);

    size_t json_len = 0;
    napi_get_value_string_utf8(env, args[2], NULL, 0, &json_len);
    char *json_buf = (char *)malloc(json_len + 1);
    if (!json_buf) {
        napi_throw_error(env, NULL, "Out of memory");
        return NULL;
    }
    napi_get_value_string_utf8(env, args[2], json_buf, json_len + 1, &json_len);

    int rc = hyle_source_entity_cache_put(dataset_id, item_id, json_buf, json_len);
    free(json_buf);

    napi_value res;
    napi_create_int32(env, rc, &res);
    return res;
}

static napi_value NapiEntityGet(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_type_error(env, NULL, "Expected (datasetId, itemId)");
        return NULL;
    }

    char dataset_id[128] = {0};
    size_t d_len = 0;
    napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);

    char item_id[128] = {0};
    size_t i_len = 0;
    napi_get_value_string_utf8(env, args[1], item_id, sizeof(item_id) - 1, &i_len);

    size_t out_len = 0;
    const char *cached = hyle_source_entity_cache_get(dataset_id, item_id, &out_len);
    if (!cached || out_len == 0) {
        napi_value null_val;
        napi_get_null(env, &null_val);
        return null_val;
    }

    napi_value res;
    napi_create_string_utf8(env, cached, out_len, &res);
    return res;
}

static napi_value NapiEntityInvalidate(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, NULL, NULL);

    char dataset_id[128] = {0};
    size_t d_len = 0;
    if (argc > 0) {
        napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);
    }

    char item_id[128] = {0};
    size_t i_len = 0;
    if (argc > 1) {
        napi_get_value_string_utf8(env, args[1], item_id, sizeof(item_id) - 1, &i_len);
    }

    hyle_source_entity_cache_invalidate(d_len > 0 ? dataset_id : NULL, i_len > 0 ? item_id : NULL);

    napi_value res;
    napi_get_boolean(env, true, &res);
    return res;
}

static napi_value NapiEntityCount(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, NULL, NULL);

    char dataset_id[128] = {0};
    size_t d_len = 0;
    if (argc > 0) {
        napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);
    }

    size_t cnt = hyle_source_entity_cache_count(d_len > 0 ? dataset_id : NULL);

    napi_value res;
    napi_create_uint32(env, (uint32_t)cnt, &res);
    return res;
}

/* ── Reactive Subscription Bus Bindings ── */

typedef struct {
    int sub_id;
    napi_env env;
    napi_ref cb_ref;
    char dataset[64];
    int active;
} napi_sub_entry_t;

#define MAX_NAPI_SUBS 32
static napi_sub_entry_t g_napi_subs[MAX_NAPI_SUBS];

static void c_sub_callback(
    const char *dataset_id,
    const char *item_id,
    hyle_source_change_type_t change_type,
    void *user_data)
{
    napi_sub_entry_t *entry = (napi_sub_entry_t *)user_data;
    if (!entry || !entry->active || !entry->cb_ref) return;

    napi_handle_scope scope;
    napi_open_handle_scope(entry->env, &scope);

    napi_value js_cb;
    napi_get_reference_value(entry->env, entry->cb_ref, &js_cb);

    napi_value args[3];
    napi_create_string_utf8(entry->env, dataset_id ? dataset_id : "", NAPI_AUTO_LENGTH, &args[0]);
    napi_create_string_utf8(entry->env, item_id ? item_id : "", NAPI_AUTO_LENGTH, &args[1]);

    const char *action_str = "update";
    if (change_type == HYLE_SOURCE_CHANGE_PUT) action_str = "put";
    else if (change_type == HYLE_SOURCE_CHANGE_DELETE) action_str = "delete";
    else if (change_type == HYLE_SOURCE_CHANGE_INVALIDATE) action_str = "invalidate";
    napi_create_string_utf8(entry->env, action_str, NAPI_AUTO_LENGTH, &args[2]);

    napi_value global;
    napi_get_global(entry->env, &global);
    napi_call_function(entry->env, global, js_cb, 3, args, NULL);

    napi_close_handle_scope(entry->env, scope);
}

static napi_value NapiSubscribe(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_type_error(env, NULL, "Expected (datasetId, callback)");
        return NULL;
    }

    char dataset_id[64] = {0};
    size_t d_len = 0;
    napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);

    napi_valuetype vt;
    napi_typeof(env, args[1], &vt);
    if (vt != napi_function) {
        napi_throw_type_error(env, NULL, "Argument 2 must be a function");
        return NULL;
    }

    size_t slot = (size_t)-1;
    for (size_t i = 0; i < MAX_NAPI_SUBS; i++) {
        if (!g_napi_subs[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == (size_t)-1) {
        napi_throw_error(env, NULL, "Maximum subscriber limit reached");
        return NULL;
    }

    napi_sub_entry_t *entry = &g_napi_subs[slot];
    entry->env = env;
    strncpy(entry->dataset, dataset_id, sizeof(entry->dataset) - 1);
    napi_create_reference(env, args[1], 1, &entry->cb_ref);

    int sub_id = hyle_source_subscribe(dataset_id, c_sub_callback, entry);
    if (sub_id <= 0) {
        napi_delete_reference(env, entry->cb_ref);
        entry->cb_ref = NULL;
        napi_throw_error(env, NULL, "Failed to register C subscriber");
        return NULL;
    }
    entry->sub_id = sub_id;
    entry->active = 1;

    napi_value res;
    napi_create_int32(env, sub_id, &res);
    return res;
}

static napi_value NapiUnsubscribe(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (argc < 1) return NULL;

    int32_t sub_id = 0;
    napi_get_value_int32(env, args[0], &sub_id);

    for (size_t i = 0; i < MAX_NAPI_SUBS; i++) {
        if (g_napi_subs[i].active && g_napi_subs[i].sub_id == sub_id) {
            hyle_source_unsubscribe(sub_id);
            if (g_napi_subs[i].cb_ref) {
                napi_delete_reference(env, g_napi_subs[i].cb_ref);
                g_napi_subs[i].cb_ref = NULL;
            }
            g_napi_subs[i].active = 0;
            break;
        }
    }
    napi_value res;
    napi_get_boolean(env, true, &res);
    return res;
}


static napi_value NapiCachePut(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_type_error(env, NULL, "Expected (datasetId, queryStr, jsonPayload)");
        return NULL;
    }

    char dataset_id[128] = {0};
    size_t d_len = 0;
    napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);

    char query_str[512] = {0};
    size_t q_len = 0;
    napi_get_value_string_utf8(env, args[1], query_str, sizeof(query_str) - 1, &q_len);

    size_t json_len = 0;
    napi_get_value_string_utf8(env, args[2], NULL, 0, &json_len);
    char *json_buf = (char *)malloc(json_len + 1);
    if (!json_buf) {
        napi_throw_error(env, NULL, "Out of memory");
        return NULL;
    }
    napi_get_value_string_utf8(env, args[2], json_buf, json_len + 1, &json_len);

    int rc = hyle_source_cache_put(dataset_id, query_str, json_buf, json_len);
    free(json_buf);

    napi_value result;
    napi_create_int32(env, rc, &result);
    return result;
}

static napi_value NapiCacheGet(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_type_error(env, NULL, "Expected (datasetId, queryStr)");
        return NULL;
    }

    char dataset_id[128] = {0};
    size_t d_len = 0;
    napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);

    char query_str[512] = {0};
    size_t q_len = 0;
    napi_get_value_string_utf8(env, args[1], query_str, sizeof(query_str) - 1, &q_len);

    size_t out_len = 0;
    const char *cached = hyle_source_cache_get(dataset_id, query_str, &out_len);
    if (!cached || out_len == 0) {
        napi_value null_val;
        napi_get_null(env, &null_val);
        return null_val;
    }

    napi_value result;
    napi_create_string_utf8(env, cached, out_len, &result);
    return result;
}

static napi_value NapiCacheInvalidate(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_type_error(env, NULL, "Expected (datasetId)");
        return NULL;
    }

    char dataset_id[128] = {0};
    size_t d_len = 0;
    napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);

    hyle_source_cache_invalidate(dataset_id);
    napi_value undef;
    napi_get_undefined(env, &undef);
    return undef;
}

static napi_value NapiCacheClear(napi_env env, napi_callback_info info) {
    (void)info;
    hyle_source_cache_clear();
    napi_value undef;
    napi_get_undefined(env, &undef);
    return undef;
}

static napi_value NapiCacheCount(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_type_error(env, NULL, "Expected (datasetId)");
        return NULL;
    }

    char dataset_id[128] = {0};
    size_t d_len = 0;
    napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);

    size_t count = hyle_source_cache_count(dataset_id);
    napi_value result;
    napi_create_uint32(env, (uint32_t)count, &result);
    return result;
}

/* ── Dataset Registration ── */

static napi_value NapiRegisterDataset(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_type_error(env, NULL, "Expected (datasetDefObject)");
        return NULL;
    }

    napi_value def_obj = args[0];

    /* 1. id */
    napi_value id_val;
    if (napi_get_named_property(env, def_obj, "id", &id_val) != napi_ok) {
        napi_throw_type_error(env, NULL, "Dataset id is required");
        return NULL;
    }
    char id_buf[128] = {0};
    size_t id_len = 0;
    napi_get_value_string_utf8(env, id_val, id_buf, sizeof(id_buf) - 1, &id_len);

    /* 2. key_field */
    char key_field_buf[64] = "id";
    bool has_key_field = false;
    napi_has_named_property(env, def_obj, "keyField", &has_key_field);
    if (has_key_field) {
        napi_value v;
        napi_get_named_property(env, def_obj, "keyField", &v);
        size_t klen = 0;
        napi_get_value_string_utf8(env, v, key_field_buf, sizeof(key_field_buf) - 1, &klen);
    }

    /* 3. items_path */
    char items_path_buf[256] = {0};
    bool has_items_path = false;
    bool has_path = false;
    napi_has_named_property(env, def_obj, "itemsPath", &has_items_path);
    napi_has_named_property(env, def_obj, "path", &has_path);
    if (has_items_path) {
        napi_value v;
        napi_get_named_property(env, def_obj, "itemsPath", &v);
        size_t plen = 0;
        napi_get_value_string_utf8(env, v, items_path_buf, sizeof(items_path_buf) - 1, &plen);
    } else if (has_path) {
        napi_value v;
        napi_get_named_property(env, def_obj, "path", &v);
        size_t plen = 0;
        napi_get_value_string_utf8(env, v, items_path_buf, sizeof(items_path_buf) - 1, &plen);
    } else {
        snprintf(items_path_buf, sizeof(items_path_buf), "./var/%s", id_buf);
    }

    /* 4. volatile flag */
    bool is_volatile = false;
    bool has_vol = false;
    napi_has_named_property(env, def_obj, "volatile", &has_vol);
    if (has_vol) {
        napi_value v;
        napi_get_named_property(env, def_obj, "volatile", &v);
        napi_get_value_bool(env, v, &is_volatile);
    }

    /* 5. fields array */
    napi_value fields_arr;
    if (napi_get_named_property(env, def_obj, "fields", &fields_arr) != napi_ok) {
        napi_throw_type_error(env, NULL, "Dataset fields array is required");
        return NULL;
    }
    uint32_t field_count = 0;
    napi_get_array_length(env, fields_arr, &field_count);
    if (field_count == 0) {
        napi_throw_type_error(env, NULL, "Dataset must declare at least one field");
        return NULL;
    }

    hyle_source_field_t *fields = (hyle_source_field_t *)calloc(field_count, sizeof(hyle_source_field_t));
    if (!fields) {
        napi_throw_error(env, NULL, "Out of memory");
        return NULL;
    }

    for (uint32_t i = 0; i < field_count; i++) {
        napi_value f_obj;
        napi_get_element(env, fields_arr, i, &f_obj);

        napi_value name_val;
        napi_get_named_property(env, f_obj, "name", &name_val);
        size_t nlen = 0;
        napi_get_value_string_utf8(env, name_val, NULL, 0, &nlen);
        char *name_str = (char *)malloc(nlen + 1);
        napi_get_value_string_utf8(env, name_val, name_str, nlen + 1, &nlen);
        fields[i].name = name_str;

        bool has_file = false;
        napi_has_named_property(env, f_obj, "file", &has_file);
        if (has_file) {
            napi_value v;
            napi_get_named_property(env, f_obj, "file", &v);
            size_t flen = 0;
            napi_get_value_string_utf8(env, v, NULL, 0, &flen);
            char *file_str = (char *)malloc(flen + 1);
            napi_get_value_string_utf8(env, v, file_str, flen + 1, &flen);
            fields[i].file = file_str;
        } else {
            fields[i].file = strdup(name_str);
        }

        bool has_type = false;
        napi_has_named_property(env, f_obj, "type", &has_type);
        if (has_type) {
            int32_t t = 0;
            napi_value v;
            napi_get_named_property(env, f_obj, "type", &v);
            napi_get_value_int32(env, v, &t);
            fields[i].type = (hyle_field_type_t)t;
        } else {
            fields[i].type = HYLE_FIELD_STRING;
        }

        bool has_req = false;
        napi_has_named_property(env, f_obj, "required", &has_req);
        if (has_req) {
            bool req = false;
            napi_value v;
            napi_get_named_property(env, f_obj, "required", &v);
            napi_get_value_bool(env, v, &req);
            fields[i].required = req ? 1 : 0;
        }

        fields[i].writable = 1;
    }

    hyle_source_def_t def;
    memset(&def, 0, sizeof(def));
    def.id = id_buf;
    def.key_field = key_field_buf;
    def.items_path = items_path_buf;
    def.fields = fields;
    def.field_count = field_count;
    def.flags = is_volatile ? HYLE_SOURCE_FLAG_VOLATILE : 0;

    corm_record_field_t *cfields = (corm_record_field_t *)calloc(field_count, sizeof(corm_record_field_t));
    if (cfields) {
        for (uint32_t i = 0; i < field_count; i++) {
            cfields[i].name = fields[i].name;
            cfields[i].type = CM_STR;
            cfields[i].offset = i * 256;
            cfields[i].max_size = 256;
        }
        uint32_t record_id = corm_record_register(id_buf, field_count * 256, cfields, field_count);
        def.record_id = record_id;
        def.record_size = field_count * 256;
    }

    if (items_path_buf[0]) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", items_path_buf);
        int sys_rc = system(cmd);
        (void)sys_rc;
    }

    int rc = hyle_source_register_def(&def);

    napi_value result;
    napi_get_boolean(env, rc == 0, &result);
    return result;
}

/* ── Item CRUD Operations ── */

static napi_value NapiUpdateItem(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_type_error(env, NULL, "Expected (datasetId, itemId, recordObject)");
        return NULL;
    }

    char dataset_id[128] = {0};
    size_t d_len = 0;
    napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);

    char item_id[256] = {0};
    size_t i_len = 0;
    napi_get_value_string_utf8(env, args[1], item_id, sizeof(item_id) - 1, &i_len);

    napi_value record_obj = args[2];
    napi_value prop_names;
    if (napi_get_property_names(env, record_obj, &prop_names) != napi_ok) {
        napi_throw_type_error(env, NULL, "Invalid record object");
        return NULL;
    }

    uint32_t prop_count = 0;
    napi_get_array_length(env, prop_names, &prop_count);

    /* Open transient corm row */
    uint32_t hd = corm_open(NULL, NULL, CM_STR, CM_STR, 0xFF, 0);
    if (hd == CM_MISS || hd == 0) {
        napi_throw_error(env, NULL, "Failed to initialize record handle");
        return NULL;
    }

    for (uint32_t i = 0; i < prop_count; i++) {
        napi_value key_val;
        napi_get_element(env, prop_names, i, &key_val);
        char key_buf[128] = {0};
        size_t klen = 0;
        napi_get_value_string_utf8(env, key_val, key_buf, sizeof(key_buf) - 1, &klen);

        napi_value val;
        napi_get_property(env, record_obj, key_val, &val);

        napi_valuetype vt;
        napi_typeof(env, val, &vt);
        char val_buf[4096] = {0};
        if (vt == napi_string) {
            size_t vlen = 0;
            napi_get_value_string_utf8(env, val, val_buf, sizeof(val_buf) - 1, &vlen);
        } else if (vt == napi_number) {
            int64_t num = 0;
            napi_get_value_int64(env, val, &num);
            snprintf(val_buf, sizeof(val_buf), "%lld", (long long)num);
        } else if (vt == napi_boolean) {
            bool b = false;
            napi_get_value_bool(env, val, &b);
            snprintf(val_buf, sizeof(val_buf), "%s", b ? "1" : "0");
        }
        corm_put(hd, key_buf, val_buf);
    }

    int rc = hyle_source_update_item(0, dataset_id, item_id, hd);
    corm_close(hd);

    napi_value result;
    napi_get_boolean(env, rc == 0, &result);
    return result;
}

static napi_value NapiDeleteItem(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_type_error(env, NULL, "Expected (datasetId, itemId)");
        return NULL;
    }

    char dataset_id[128] = {0};
    size_t d_len = 0;
    napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);

    char item_id[256] = {0};
    size_t i_len = 0;
    napi_get_value_string_utf8(env, args[1], item_id, sizeof(item_id) - 1, &i_len);

    const hyle_source_def_t *def = hyle_source_find(dataset_id);
    if (!def) {
        napi_value res;
        napi_get_boolean(env, false, &res);
        return res;
    }

    int rc = hyle_source_delete_item(0, def, item_id);
    napi_value result;
    napi_get_boolean(env, rc == 0, &result);
    return result;
}

static napi_value NapiItemExists(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_type_error(env, NULL, "Expected (datasetId, itemId)");
        return NULL;
    }

    char dataset_id[128] = {0};
    size_t d_len = 0;
    napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);

    char item_id[256] = {0};
    size_t i_len = 0;
    napi_get_value_string_utf8(env, args[1], item_id, sizeof(item_id) - 1, &i_len);

    int exists = hyle_source_item_exists(dataset_id, item_id);
    napi_value result;
    napi_get_boolean(env, exists != 0, &result);
    return result;
}

static napi_value NapiGetField(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_type_error(env, NULL, "Expected (datasetId, itemId, fieldName)");
        return NULL;
    }

    char dataset_id[128] = {0};
    size_t d_len = 0;
    napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);

    char item_id[256] = {0};
    size_t i_len = 0;
    napi_get_value_string_utf8(env, args[1], item_id, sizeof(item_id) - 1, &i_len);

    char field_name[128] = {0};
    size_t f_len = 0;
    napi_get_value_string_utf8(env, args[2], field_name, sizeof(field_name) - 1, &f_len);

    const char *val = hyle_source_get_field(dataset_id, item_id, field_name);
    if (!val) {
        napi_value null_val;
        napi_get_null(env, &null_val);
        return null_val;
    }

    napi_value result;
    napi_create_string_utf8(env, val, strlen(val), &result);
    return result;
}

static napi_value NapiQueryDataset(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_type_error(env, NULL, "Expected (datasetId, queryStr)");
        return NULL;
    }

    char dataset_id[128] = {0};
    size_t d_len = 0;
    napi_get_value_string_utf8(env, args[0], dataset_id, sizeof(dataset_id) - 1, &d_len);

    char query_str[512] = {0};
    size_t q_len = 0;
    napi_get_value_string_utf8(env, args[1], query_str, sizeof(query_str) - 1, &q_len);

    unsigned qh = hyle_source_query_dataset(dataset_id, query_str);

    napi_value res_obj;
    napi_create_object(env, &res_obj);

    napi_value hd_val;
    napi_create_uint32(env, (uint32_t)qh, &hd_val);
    napi_set_named_property(env, res_obj, "handle", hd_val);

    uint32_t total = 0;
    napi_value ids_arr;
    napi_create_array(env, &ids_arr);

    if (qh && qh != CM_MISS) {
        const char *total_str = (const char *)corm_get(qh, "__total__");
        total = total_str ? (uint32_t)atoi(total_str) : 0;

        uint32_t arr_idx = 0;
        uint32_t cur = corm_iter(qh, NULL, 0);
        const void *rkey = NULL;
        const void *rval = NULL;
        while (corm_next(&rkey, &rval, cur)) {
            if (!rkey || strcmp((const char *)rkey, "__total__") == 0) continue;
            napi_value id_str;
            napi_create_string_utf8(env, (const char *)rkey, strlen((const char *)rkey), &id_str);
            napi_set_element(env, ids_arr, arr_idx++, id_str);
        }
        corm_fin(cur);
        corm_close(qh);
    }

    napi_value total_val;
    napi_create_uint32(env, total, &total_val);
    napi_set_named_property(env, res_obj, "total", total_val);
    napi_set_named_property(env, res_obj, "ids", ids_arr);

    return res_obj;
}

/* ── Module Export Registration ── */

#define NAPI_EXPORT_FN(name, fn) \
    do { \
        napi_value fn_val; \
        napi_create_function(env, NULL, 0, fn, NULL, &fn_val); \
        napi_set_named_property(env, exports, name, fn_val); \
    } while (0)

NAPI_MODULE_INIT() {
    NAPI_EXPORT_FN("normalizeQuery", NapiNormalizeQuery);
    NAPI_EXPORT_FN("cachePutTtl", NapiCachePutTtl);
    NAPI_EXPORT_FN("cacheHydrate", NapiCacheHydrate);
    NAPI_EXPORT_FN("cachePut", NapiCachePut);
    NAPI_EXPORT_FN("cacheGetStatus", NapiCacheGetStatus);
    NAPI_EXPORT_FN("cacheGet", NapiCacheGet);
    NAPI_EXPORT_FN("cacheInvalidate", NapiCacheInvalidate);
    NAPI_EXPORT_FN("cacheClear", NapiCacheClear);
    NAPI_EXPORT_FN("cacheCount", NapiCacheCount);
    NAPI_EXPORT_FN("entityPut", NapiEntityPut);
    NAPI_EXPORT_FN("entityGet", NapiEntityGet);
    NAPI_EXPORT_FN("entityInvalidate", NapiEntityInvalidate);
    NAPI_EXPORT_FN("entityCount", NapiEntityCount);
    NAPI_EXPORT_FN("subscribe", NapiSubscribe);
    NAPI_EXPORT_FN("unsubscribe", NapiUnsubscribe);
    NAPI_EXPORT_FN("registerDataset", NapiRegisterDataset);
    NAPI_EXPORT_FN("updateItem", NapiUpdateItem);
    NAPI_EXPORT_FN("deleteItem", NapiDeleteItem);
    NAPI_EXPORT_FN("itemExists", NapiItemExists);
    NAPI_EXPORT_FN("getField", NapiGetField);
    NAPI_EXPORT_FN("queryDataset", NapiQueryDataset);
    return exports;
}
