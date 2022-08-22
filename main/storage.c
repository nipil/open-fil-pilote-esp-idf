#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_check.h>
#include <esp_partition.h>
#include <nvs_flash.h>
#include <nvs.h>

#include "storage.h"

static const char TAG[] = "storage";

static const char default_nvs_partition_name[] = NVS_DEFAULT_PART_NAME;

static const char null_str[] = "NULL";

/* partition functions */

void part_list(void)
{
    esp_partition_iterator_t it_p = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    ESP_LOGD(TAG, "Partition found, listing them...");
    while (it_p != NULL)
    {
        const esp_partition_t *part = esp_partition_get(it_p);
        ESP_LOGI(TAG, "Partition: %s (type=%d, subtype=%d)", part->label, part->type, part->subtype);
        it_p = esp_partition_next(it_p);
    }
}

/* NVS backend management */

void kv_erase(const char *part_name)
{
    if (part_name == NULL)
        part_name = default_nvs_partition_name;
    ESP_LOGW(TAG, "Erasing flash... Every settings will be deleted.");
    esp_err_t err = nvs_flash_erase_partition(part_name);
    ESP_LOGD(TAG, "nvs_flash_erase: %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_stats(const char *part_name)
{
    if (part_name == NULL)
        part_name = default_nvs_partition_name;
    nvs_stats_t nvs_stats;
    esp_err_t err = nvs_get_stats(part_name, &nvs_stats);
    ESP_LOGD(TAG, "nvs_get_stats: %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Storage stats failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "NVS stats: Used %d, Free = (%d), All = (%d)",
             nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);
}

void kv_init(const char *part_name)
{
    if (part_name == NULL)
        part_name = default_nvs_partition_name;
    esp_err_t err = nvs_flash_init_partition(part_name);
    ESP_LOGD(TAG, "nvs_flash_init: %s", esp_err_to_name(err));
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGI(TAG, "Mounting error: %s", esp_err_to_name(err));
        // NVS partition was truncated and needs to be erased
        kv_erase(part_name);
        // Retry nvs_flash_init
        ESP_LOGI(TAG, "Retrying mounting flash...");
        err = nvs_flash_init();
        ESP_LOGD(TAG, "nvs_flash_init: %s", esp_err_to_name(err));
    }
    ESP_ERROR_CHECK(err);
}

void kv_deinit(const char *part_name)
{
    if (part_name == NULL)
        part_name = default_nvs_partition_name;
    esp_err_t err = nvs_flash_deinit_partition(part_name);
    ESP_ERROR_CHECK(err);
}

/* all-in-one functions */
void kv_clear_ns(const char *ns)
{
    ESP_LOGD(TAG, "Clear namespace %s", (ns != NULL) ? ns : null_str);
    assert(ns != NULL);
    nvs_handle handle = kv_open_ns(ns);
    esp_err_t err = nvs_erase_all(handle);
    ESP_ERROR_CHECK(err);
    kv_commit(handle);
    kv_close(handle);
}

void kv_delete_key_ns(const char *ns, const char *key)
{
    ESP_LOGD(TAG, "Delete key '%s' from namespace %s", (key != NULL) ? key : null_str, (ns != NULL) ? ns : null_str);
    assert(ns != NULL);
    nvs_handle handle = kv_open_ns(ns);
    kv_delete_key(handle, key);
    kv_commit(handle);
    kv_close(handle);
}

/*
 * Lists key-value in partition name and namespace
 * if part_name is NULL, then use default_nvs_partition_name
 * if ns is NULL, list key-values in all namespaces
 */
void kv_list_ns(const char *part_name, const char *ns)
{
    if (part_name == NULL)
    {
        part_name = default_nvs_partition_name;
    }
    nvs_iterator_t it = nvs_entry_find(part_name, ns, NVS_TYPE_ANY);
    while (it != NULL)
    {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        ESP_LOGD(TAG, "Namespace '%s', key '%s', type '%d'", info.namespace_name, info.key, info.type);
        it = nvs_entry_next(it);
    }
    nvs_release_iterator(it);
}

nvs_handle_t kv_open_ns(const char *ns)
{
    ESP_LOGD(TAG, "Open namespace %s", (ns != NULL) ? ns : null_str);
    assert(ns != NULL);
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    ESP_LOGD(TAG, "nvs_open: %s handle %u", esp_err_to_name(err), handle);
    ESP_ERROR_CHECK(err);
    return handle;
}

void kv_commit(nvs_handle_t handle)
{
    ESP_LOGD(TAG, "Committing handle %u", handle);
    esp_err_t err = nvs_commit(handle);
    ESP_LOGD(TAG, "nvs_commit: %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_close(nvs_handle_t handle)
{
    ESP_LOGD(TAG, "Closing handle %u", handle);
    nvs_close(handle);
}

void kv_delete_key(nvs_handle_t handle, const char *key)
{
    assert(key != NULL);
    assert(strlen(key) != 0);
    esp_err_t err = nvs_erase_key(handle, key);
    ESP_LOGD(TAG, "nvs_erase_key %s", esp_err_to_name(err));
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "NVS delete key %s not found", key);
        return;
    }
    ESP_ERROR_CHECK(err);
}

void kv_clear(nvs_handle_t handle)
{
    ESP_LOGD(TAG, "Clear opened namespace");
    esp_err_t err = nvs_erase_all(handle);
    ESP_ERROR_CHECK(err);
}

/* NVS setters */

void kv_set_i8(nvs_handle_t handle, const char *key, int8_t value)
{
    ESP_LOGD(TAG, "kv_set_i8 key=%s value=%i", key, value);
    esp_err_t err = nvs_set_i8(handle, key, value);
    ESP_LOGD(TAG, "nvs_set_i8 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_u8(nvs_handle_t handle, const char *key, uint8_t value)
{
    ESP_LOGD(TAG, "kv_set_u8 key=%s value=%u", key, value);
    esp_err_t err = nvs_set_u8(handle, key, value);
    ESP_LOGD(TAG, "nvs_set_u8 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_i16(nvs_handle_t handle, const char *key, int16_t value)
{
    ESP_LOGD(TAG, "kv_set_i16 key=%s value=%i", key, value);
    esp_err_t err = nvs_set_i16(handle, key, value);
    ESP_LOGD(TAG, "nvs_set_i16 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_u16(nvs_handle_t handle, const char *key, uint16_t value)
{
    ESP_LOGD(TAG, "kv_set_u16 key=%s value=%u", key, value);
    esp_err_t err = nvs_set_u16(handle, key, value);
    ESP_LOGD(TAG, "nvs_set_u16 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_i32(nvs_handle_t handle, const char *key, int32_t value)
{
    ESP_LOGD(TAG, "kv_set_i32 key=%s value=%i", key, value);
    esp_err_t err = nvs_set_i32(handle, key, value);
    ESP_LOGD(TAG, "nvs_set_i32 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_u32(nvs_handle_t handle, const char *key, uint32_t value)
{
    ESP_LOGD(TAG, "kv_set_u32 key=%s value=%u", key, value);
    esp_err_t err = nvs_set_u32(handle, key, value);
    ESP_LOGD(TAG, "nvs_set_u32 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_i64(nvs_handle_t handle, const char *key, int64_t value)
{
    ESP_LOGD(TAG, "kv_set_i64 key=%s value=%lli", key, value);
    esp_err_t err = nvs_set_i64(handle, key, value);
    ESP_LOGD(TAG, "nvs_set_i64 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_u64(nvs_handle_t handle, const char *key, uint64_t value)
{
    ESP_LOGD(TAG, "kv_set_u64 key=%s value=%llu", key, value);
    esp_err_t err = nvs_set_u64(handle, key, value);
    ESP_LOGD(TAG, "nvs_set_u64 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_str(nvs_handle_t handle, const char *key, const char *value)
{
    ESP_LOGD(TAG, "kv_set_str key=%s value=%s", key, (value != NULL) ? value : null_str);
    assert(value != NULL);
    esp_err_t err = nvs_set_str(handle, key, value);
    ESP_LOGD(TAG, "nvs_set_str %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length)
{
    assert(value != NULL);
    ESP_LOGD(TAG, "kv_set_blob key=%s value=%p length=%u", key, value, length);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, value, length, ESP_LOG_DEBUG);

    esp_err_t err = nvs_set_blob(handle, key, value, length);
    ESP_LOGD(TAG, "nvs_set_blob %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

/* NVS getters */

int8_t kv_get_i8(nvs_handle_t handle, const char *key, int8_t def_value)
{
    ESP_LOGD(TAG, "kv_get_i8 key=%s def=%i", key, def_value);
    int8_t value;
    esp_err_t err = nvs_get_i8(handle, key, &value);
    ESP_LOGD(TAG, "nvs_get_i8 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "nvs_get_i8 error, defaulting");
        value = def_value;
    }
    ESP_LOGD(TAG, "kv_get_i8 ret=%i", value);
    return value;
}

uint8_t kv_get_u8(nvs_handle_t handle, const char *key, uint8_t def_value)
{
    ESP_LOGD(TAG, "kv_get_u8 key=%s def=%u", key, def_value);
    uint8_t value;
    esp_err_t err = nvs_get_u8(handle, key, &value);
    ESP_LOGD(TAG, "nvs_get_u8 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "nvs_get_u8 error, defaulting");
        value = def_value;
    }
    ESP_LOGD(TAG, "kv_get_u8 ret=%u", value);
    return value;
}

int16_t kv_get_i16(nvs_handle_t handle, const char *key, int16_t def_value)
{
    ESP_LOGD(TAG, "kv_get_i16 key=%s def=%i", key, def_value);
    int16_t value;
    esp_err_t err = nvs_get_i16(handle, key, &value);
    ESP_LOGD(TAG, "nvs_get_i16 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "nvs_get_i16 error, defaulting");
        value = def_value;
    }
    ESP_LOGD(TAG, "kv_get_i16 ret=%i", value);
    return value;
}

uint16_t kv_get_u16(nvs_handle_t handle, const char *key, uint16_t def_value)
{
    ESP_LOGD(TAG, "kv_get_u16 key=%s def=%u", key, def_value);
    uint16_t value;
    esp_err_t err = nvs_get_u16(handle, key, &value);
    ESP_LOGD(TAG, "nvs_get_u16 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "nvs_get_u16 error, defaulting");
        value = def_value;
    }
    ESP_LOGD(TAG, "kv_get_u16 ret=%u", value);
    return value;
}

int32_t kv_get_i32(nvs_handle_t handle, const char *key, int32_t def_value)
{
    ESP_LOGD(TAG, "kv_get_i32 key=%s def=%i", key, def_value);
    int32_t value;
    esp_err_t err = nvs_get_i32(handle, key, &value);
    ESP_LOGD(TAG, "nvs_get_i32 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "nvs_get_i32 error, defaulting");
        value = def_value;
    }
    ESP_LOGD(TAG, "kv_get_i32 ret=%i", value);
    return value;
}

uint32_t kv_get_u32(nvs_handle_t handle, const char *key, uint32_t def_value)
{
    ESP_LOGD(TAG, "kv_get_u32 key=%s def=%u", key, def_value);
    uint32_t value;
    esp_err_t err = nvs_get_u32(handle, key, &value);
    ESP_LOGD(TAG, "nvs_get_u32 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "nvs_get_u32 error, defaulting");
        value = def_value;
    }
    ESP_LOGD(TAG, "kv_get_u32 ret=%u", value);
    return value;
}

int64_t kv_get_i64(nvs_handle_t handle, const char *key, int64_t def_value)
{
    ESP_LOGD(TAG, "kv_get_i64 key=%s def=%lli", key, def_value);
    int64_t value;
    esp_err_t err = nvs_get_i64(handle, key, &value);
    ESP_LOGD(TAG, "nvs_get_i64 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "nvs_get_i64 error, defaulting");
        value = def_value;
    }
    ESP_LOGD(TAG, "kv_get_i64 ret=%lli", value);
    return value;
}

uint64_t kv_get_u64(nvs_handle_t handle, const char *key, uint64_t def_value)
{
    ESP_LOGD(TAG, "kv_get_u64 key=%s def=%llu", key, def_value);
    uint64_t value;
    esp_err_t err = nvs_get_u64(handle, key, &value);
    ESP_LOGD(TAG, "nvs_get_u64 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "nvs_get_u64 error, defaulting");
        value = def_value;
    }
    ESP_LOGD(TAG, "kv_get_u64 ret=%llu", value);
    return value;
}

/* Returned memory MUST BE FREED (or owned) BY THE CALLER */
char *kv_get_str(nvs_handle_t handle, const char *key)
{
    ESP_LOGD(TAG, "kv_get_str key=%s", key);
    char *buf;
    size_t len;

    // get string length (documentation says it includes terminating NULL character)
    esp_err_t err = nvs_get_str(handle, key, NULL, &len);
    ESP_LOGD(TAG, "nvs_get_str %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "nvs_get_str error");
        return NULL;
    }

    // allocate memory
    ESP_LOGD(TAG, "nvs_get_str len=%u (includes \\0)", len);
    buf = (char *)malloc(len);
    assert(buf != NULL);
    if (buf == NULL) // if asserts are disabled
    {
        ESP_LOGW(TAG, "kv_get_str malloc failed");
        return NULL;
    }

    // get actual string
    err = nvs_get_str(handle, key, buf, &len);
    ESP_LOGD(TAG, "nvs_get_str %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        free(buf);
        ESP_LOGW(TAG, "nvs_get_str error");
        return NULL;
    }

    ESP_LOGD(TAG, "kv_get_str ret=%s", buf);
    return buf; // caller MUST FREE returned value if not NULL
}

/* Returned memory MUST BE FREED (or owned) BY THE CALLER */
void *kv_get_blob(nvs_handle_t handle, const char *key, size_t *length)
{
    assert(length != NULL);
    if (length == NULL) // if asserts are disabled
    {
        return NULL;
    }

    // fail safe
    *length = 0;

    ESP_LOGD(TAG, "kv_get_blob key=%s", key);
    void *buf;
    size_t buf_len;

    // get blob length
    esp_err_t err = nvs_get_blob(handle, key, NULL, &buf_len);
    ESP_LOGD(TAG, "nvs_get_blob %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "nvs_get_blob error");
        return NULL;
    }

    // allocate memory
    ESP_LOGD(TAG, "nvs_get_blob len=%u", buf_len);
    buf = malloc(buf_len);
    assert(buf != NULL);
    if (buf == NULL) // if asserts are disabled
    {
        ESP_LOGW(TAG, "kv_get_blob malloc failed");
        return NULL;
    }

    // get actual string
    err = nvs_get_blob(handle, key, buf, &buf_len);
    ESP_LOGD(TAG, "nvs_get_blob %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        free(buf);
        ESP_LOGW(TAG, "nvs_get_blob error");
        return NULL;
    }

    *length = buf_len;
    ESP_LOGD(TAG, "kv_get_str len=%u ret=%p", buf_len, buf);
    return buf; // caller MUST FREE returned value if not NULL
}

/* unit tests */
void test_storage(void)
{
    const char ns[] = "test_storage";

    uint8_t ui8;
    char *buf;
    void *p_blob;
    size_t len;

    ESP_LOGD(TAG, "==== general RAW =========================");
    part_list();
    kv_init(NULL);  // default partition
    kv_stats(NULL); // default partition

    ESP_LOGD(TAG, "---- ");
    kv_list_ns(NULL, NULL); // default partition, every namespace
    ESP_LOGD(TAG, "---- ");
    kv_clear_ns(ns); // default partition, specific namespace
    ESP_LOGD(TAG, "---- ");
    kv_list_ns(NULL, NULL); // default partition, every namespace

    ESP_LOGD(TAG, "==== test storing data RAW =========================");

    nvs_handle_t h = kv_open_ns(ns);
    kv_set_i8(h, "i8", INT8_MIN);
    kv_set_u8(h, "u8", UINT8_MAX);
    kv_set_i16(h, "i16", INT16_MIN);
    kv_set_u16(h, "u16", UINT16_MAX);
    kv_set_i32(h, "i32", INT32_MIN);
    kv_set_u32(h, "u32", UINT32_MAX);
    kv_set_i64(h, "i64", INT64_MIN);
    kv_set_u64(h, "u64", UINT64_MAX);
    kv_set_str(h, "str", "foo");
    uint8_t data[] = {0x69, 0x51};
    kv_set_blob(h, "blob", data, sizeof(data));
    kv_commit(h);
    kv_close(h);

    ESP_LOGD(TAG, "==== test storing data MACRO =======================");
    kvh_set(u8, ns, "mu8", 69);
    kvh_set(str, ns, "mstr", "toto");
    kvh_set(blob, ns, "mblob", data, sizeof(data));

    /* test reading data */

    // should return stored
    ESP_LOGD(TAG, "==== test reading stored RAW =========================");
    h = kv_open_ns(ns);
    kv_get_i8(h, "i8", -1);
    kv_get_u8(h, "u8", +1);
    kv_get_i16(h, "i16", -1);
    kv_get_u16(h, "u16", +1);
    kv_get_i32(h, "i32", -1);
    kv_get_u32(h, "u32", +1);
    kv_get_i64(h, "i64", -1);
    kv_get_u64(h, "u64", +1);
    buf = kv_get_str(h, "str");
    if (buf != NULL)
    {
        ESP_LOGD(TAG, "%s", buf);
        free(buf);
    }
    p_blob = kv_get_blob(h, "blob", &len);
    ESP_LOGD(TAG, "blob %p", p_blob);
    if (p_blob != NULL)
    {
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, p_blob, len, ESP_LOG_DEBUG);
        free(p_blob);
    }
    kv_close(h);

    ESP_LOGD(TAG, "==== test reading stored MACRO =========================");
    kvh_get(ui8, u8, ns, "mu8", 42);
    ESP_LOGD(TAG, "%i", ui8);
    kvh_get(buf, str, ns, "mstr");
    if (buf != NULL)
    {
        ESP_LOGD(TAG, "%s", buf);
        free(buf);
    }
    kvh_get(p_blob, blob, ns, "mblob", &len);
    if (p_blob != NULL)
    {
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, p_blob, len, ESP_LOG_DEBUG);
        free(p_blob);
    }

    ESP_LOGD(TAG, "==== test read defaults RAW =========================");
    h = kv_open_ns(ns);
    kv_get_i8(h, "_i8", -1);
    kv_get_u8(h, "_u8", +1);
    kv_get_i16(h, "_i16", -1);
    kv_get_u16(h, "_u16", +1);
    kv_get_i32(h, "_i32", -1);
    kv_get_u32(h, "_u32", +1);
    kv_get_i64(h, "_i64", -1);
    kv_get_u64(h, "_u64", +1);
    buf = kv_get_str(h, "_str");
    if (buf != NULL)
    {
        ESP_LOGD(TAG, "%s", buf);
        free(buf);
    }
    p_blob = kv_get_blob(h, "_blob", &len);
    ESP_LOGD(TAG, "p_blob %p", p_blob);
    if (p_blob != NULL)
    {
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, p_blob, len, ESP_LOG_DEBUG);
        free(p_blob);
    }
    kv_close(h);

    ESP_LOGD(TAG, "==== test read defaults MACRO =========================");
    kvh_get(ui8, u8, ns, "_mu8", 42);
    ESP_LOGD(TAG, "%i", ui8);
    kvh_get(buf, str, ns, "_mstr");
    if (buf != NULL)
    {
        ESP_LOGD(TAG, "%s but should be NULL", buf);
        free(buf);
    }
    kvh_get(p_blob, blob, ns, "_mblob", &len);
    if (p_blob != NULL)
    {
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, p_blob, len, ESP_LOG_DEBUG);
        ESP_LOGW(TAG, "blob should be NULL");
        free(p_blob);
    }

    ESP_LOGD(TAG, "==== VALUES =========================");
    kv_list_ns(NULL, ns); // default partition, specific namespace
    ESP_LOGD(TAG, "---- ");
    h = kv_open_ns(ns);
    kv_delete_key(h, "mblob");
    kv_close(h);
    ESP_LOGD(TAG, "---- ");
    kv_delete_key_ns(ns, "mstr");
    ESP_LOGD(TAG, "---- ");
    kv_list_ns(NULL, ns); // default partition, specific namespace
}