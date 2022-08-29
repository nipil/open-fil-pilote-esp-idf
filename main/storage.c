#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_check.h>
#include <nvs_flash.h>
#include <nvs.h>

#include "str.h"
#include "storage.h"

static const char TAG[] = "storage";

/* types */
struct kv_type_str_pair
{
    nvs_type_t type;
    const char *str;
};

/* constants */

static const struct kv_type_str_pair kv_type_str[] = {
    {NVS_TYPE_I8, "i8"},
    {NVS_TYPE_U8, "u8"},
    {NVS_TYPE_U16, "u16"},
    {NVS_TYPE_I16, "i16"},
    {NVS_TYPE_U32, "u32"},
    {NVS_TYPE_I32, "i32"},
    {NVS_TYPE_U64, "u64"},
    {NVS_TYPE_I64, "i64"},
    {NVS_TYPE_STR, "str"},
    {NVS_TYPE_BLOB, "blob"},
    {NVS_TYPE_ANY, "any"},
};

const char *kv_type_str_from_nvs_type(nvs_type_t type)
{
    static const size_t KV_TYPE_STR_PAIR_SIZE = sizeof(kv_type_str) / sizeof(kv_type_str[0]);
    for (int i = 0; i < KV_TYPE_STR_PAIR_SIZE; i++)
        if (kv_type_str[i].type == type)
            return kv_type_str[i].str;
    return NULL;
}

/* namespaces */

static const char ns_ofp[] = "ofp";
static char ns_ofp_hw[NVS_NS_NAME_MAX_SIZE] = {0};
static char ns_ofp_zn[NVS_NS_NAME_MAX_SIZE] = {0};

bool kv_build_ns_hardware(const char *hw_id, char *buf)
{
    int n = snprintf(buf, NVS_NS_NAME_MAX_SIZE, "ofp_hw_%s", hw_id);
    return (n >= 0 && n < NVS_NS_NAME_MAX_SIZE);
}

bool kv_set_ns_current_hardware(const char *hw_id)
{
    return kv_build_ns_hardware(hw_id, ns_ofp_hw);
}

const char *kv_get_ns_hardware(void)
{
    return ns_ofp_hw;
}

bool kv_build_ns_zone(const char *hw_id, char *buf)
{
    int n = snprintf(buf, NVS_NS_NAME_MAX_SIZE, "ofp_zn_%s", hw_id);
    return (n >= 0 && n < NVS_NS_NAME_MAX_SIZE);
}

bool kv_set_ns_current_zone(const char *hw_id)
{
    return kv_build_ns_zone(hw_id, ns_ofp_zn);
}

const char *kv_get_ns_zone(void)
{
    return ns_ofp_zn;
}

const char *kv_get_ns_ofp(void)
{
    return ns_ofp;
}

/* NVS backend management */

void kv_erase(const char *part_name)
{
    if (part_name == NULL)
        part_name = default_nvs_partition_name;
    ESP_LOGW(TAG, "Erasing flash... Every settings will be deleted.");
    esp_err_t err = nvs_flash_erase_partition(part_name);
    ESP_LOGV(TAG, "nvs_flash_erase: %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_init(const char *part_name)
{
    // esp_log_level_set(TAG, ESP_LOG_VERBOSE); // debug
    if (part_name == NULL)
        part_name = default_nvs_partition_name;
    esp_err_t err = nvs_flash_init_partition(part_name);
    ESP_LOGV(TAG, "nvs_flash_init: %s", esp_err_to_name(err));
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "Mounting error: %s", esp_err_to_name(err));
        // NVS partition was truncated and needs to be erased
        kv_erase(part_name);
        // Retry nvs_flash_init
        ESP_LOGI(TAG, "Retrying mounting flash...");
        err = nvs_flash_init();
        ESP_LOGV(TAG, "nvs_flash_init: %s", esp_err_to_name(err));
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

nvs_handle_t kv_open_ns(const char *ns)
{
    ESP_LOGD(TAG, "Open namespace %s", (ns != NULL) ? ns : null_str);
    assert(ns != NULL);
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    ESP_LOGV(TAG, "nvs_open: %s handle %u", esp_err_to_name(err), handle);
    ESP_ERROR_CHECK(err);
    return handle;
}

void kv_commit(nvs_handle_t handle)
{
    ESP_LOGD(TAG, "Committing handle %u", handle);
    esp_err_t err = nvs_commit(handle);
    ESP_LOGV(TAG, "nvs_commit: %s", esp_err_to_name(err));
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
    ESP_LOGD(TAG, "Deleting key %s", key);

    esp_err_t err = nvs_erase_key(handle, key);
    ESP_LOGV(TAG, "nvs_erase_key %s", esp_err_to_name(err));
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGV(TAG, "NVS delete key %s not found", key);
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
    ESP_LOGV(TAG, "nvs_set_i8 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_u8(nvs_handle_t handle, const char *key, uint8_t value)
{
    ESP_LOGD(TAG, "kv_set_u8 key=%s value=%u", key, value);
    esp_err_t err = nvs_set_u8(handle, key, value);
    ESP_LOGV(TAG, "nvs_set_u8 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_i16(nvs_handle_t handle, const char *key, int16_t value)
{
    ESP_LOGD(TAG, "kv_set_i16 key=%s value=%i", key, value);
    esp_err_t err = nvs_set_i16(handle, key, value);
    ESP_LOGV(TAG, "nvs_set_i16 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_u16(nvs_handle_t handle, const char *key, uint16_t value)
{
    ESP_LOGD(TAG, "kv_set_u16 key=%s value=%u", key, value);
    esp_err_t err = nvs_set_u16(handle, key, value);
    ESP_LOGV(TAG, "nvs_set_u16 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_i32(nvs_handle_t handle, const char *key, int32_t value)
{
    ESP_LOGD(TAG, "kv_set_i32 key=%s value=%i", key, value);
    esp_err_t err = nvs_set_i32(handle, key, value);
    ESP_LOGV(TAG, "nvs_set_i32 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_u32(nvs_handle_t handle, const char *key, uint32_t value)
{
    ESP_LOGD(TAG, "kv_set_u32 key=%s value=%u", key, value);
    esp_err_t err = nvs_set_u32(handle, key, value);
    ESP_LOGV(TAG, "nvs_set_u32 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_i64(nvs_handle_t handle, const char *key, int64_t value)
{
    ESP_LOGD(TAG, "kv_set_i64 key=%s value=%lli", key, value);
    esp_err_t err = nvs_set_i64(handle, key, value);
    ESP_LOGV(TAG, "nvs_set_i64 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_u64(nvs_handle_t handle, const char *key, uint64_t value)
{
    ESP_LOGD(TAG, "kv_set_u64 key=%s value=%llu", key, value);
    esp_err_t err = nvs_set_u64(handle, key, value);
    ESP_LOGV(TAG, "nvs_set_u64 %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_str(nvs_handle_t handle, const char *key, const char *value)
{
    ESP_LOGD(TAG, "kv_set_str key=%s value=%s", key, (value != NULL) ? value : null_str);
    assert(value != NULL);
    esp_err_t err = nvs_set_str(handle, key, value);
    ESP_LOGV(TAG, "nvs_set_str %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length)
{
    assert(value != NULL);
    ESP_LOGD(TAG, "kv_set_blob key=%s value=%p length=%u", key, value, length);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, value, length, ESP_LOG_DEBUG);

    esp_err_t err = nvs_set_blob(handle, key, value, length);
    ESP_LOGV(TAG, "nvs_set_blob %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

/* NVS getters */

int8_t kv_get_i8(nvs_handle_t handle, const char *key, int8_t def_value)
{
    ESP_LOGD(TAG, "kv_get_i8 key=%s def=%i", key, def_value);
    int8_t value;
    esp_err_t err = nvs_get_i8(handle, key, &value);
    ESP_LOGV(TAG, "nvs_get_i8 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGV(TAG, "nvs_get_i8 error, defaulting");
        value = def_value;
    }
    ESP_LOGV(TAG, "kv_get_i8 ret=%i", value);
    return value;
}

uint8_t kv_get_u8(nvs_handle_t handle, const char *key, uint8_t def_value)
{
    ESP_LOGD(TAG, "kv_get_u8 key=%s def=%u", key, def_value);
    uint8_t value;
    esp_err_t err = nvs_get_u8(handle, key, &value);
    ESP_LOGV(TAG, "nvs_get_u8 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGV(TAG, "nvs_get_u8 error, defaulting");
        value = def_value;
    }
    ESP_LOGV(TAG, "kv_get_u8 ret=%u", value);
    return value;
}

int16_t kv_get_i16(nvs_handle_t handle, const char *key, int16_t def_value)
{
    ESP_LOGD(TAG, "kv_get_i16 key=%s def=%i", key, def_value);
    int16_t value;
    esp_err_t err = nvs_get_i16(handle, key, &value);
    ESP_LOGV(TAG, "nvs_get_i16 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGV(TAG, "nvs_get_i16 error, defaulting");
        value = def_value;
    }
    ESP_LOGV(TAG, "kv_get_i16 ret=%i", value);
    return value;
}

uint16_t kv_get_u16(nvs_handle_t handle, const char *key, uint16_t def_value)
{
    ESP_LOGD(TAG, "kv_get_u16 key=%s def=%u", key, def_value);
    uint16_t value;
    esp_err_t err = nvs_get_u16(handle, key, &value);
    ESP_LOGV(TAG, "nvs_get_u16 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGV(TAG, "nvs_get_u16 error, defaulting");
        value = def_value;
    }
    ESP_LOGV(TAG, "kv_get_u16 ret=%u", value);
    return value;
}

int32_t kv_get_i32(nvs_handle_t handle, const char *key, int32_t def_value)
{
    ESP_LOGD(TAG, "kv_get_i32 key=%s def=%i", key, def_value);
    int32_t value;
    esp_err_t err = nvs_get_i32(handle, key, &value);
    ESP_LOGV(TAG, "nvs_get_i32 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGV(TAG, "nvs_get_i32 error, defaulting");
        value = def_value;
    }
    ESP_LOGV(TAG, "kv_get_i32 ret=%i", value);
    return value;
}

uint32_t kv_get_u32(nvs_handle_t handle, const char *key, uint32_t def_value)
{
    ESP_LOGD(TAG, "kv_get_u32 key=%s def=%u", key, def_value);
    uint32_t value;
    esp_err_t err = nvs_get_u32(handle, key, &value);
    ESP_LOGV(TAG, "nvs_get_u32 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGV(TAG, "nvs_get_u32 error, defaulting");
        value = def_value;
    }
    ESP_LOGV(TAG, "kv_get_u32 ret=%u", value);
    return value;
}

int64_t kv_get_i64(nvs_handle_t handle, const char *key, int64_t def_value)
{
    ESP_LOGD(TAG, "kv_get_i64 key=%s def=%lli", key, def_value);
    int64_t value;
    esp_err_t err = nvs_get_i64(handle, key, &value);
    ESP_LOGV(TAG, "nvs_get_i64 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGV(TAG, "nvs_get_i64 error, defaulting");
        value = def_value;
    }
    ESP_LOGV(TAG, "kv_get_i64 ret=%lli", value);
    return value;
}

uint64_t kv_get_u64(nvs_handle_t handle, const char *key, uint64_t def_value)
{
    ESP_LOGD(TAG, "kv_get_u64 key=%s def=%llu", key, def_value);
    uint64_t value;
    esp_err_t err = nvs_get_u64(handle, key, &value);
    ESP_LOGV(TAG, "nvs_get_u64 %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGV(TAG, "nvs_get_u64 error, defaulting");
        value = def_value;
    }
    ESP_LOGV(TAG, "kv_get_u64 ret=%llu", value);
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
    ESP_LOGV(TAG, "nvs_get_str %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGV(TAG, "nvs_get_str error");
        return NULL;
    }

    // allocate memory
    ESP_LOGV(TAG, "nvs_get_str len=%u (includes \\0)", len);
    buf = (char *)malloc(len);
    assert(buf != NULL);
    if (buf == NULL) // if asserts are disabled
    {
        ESP_LOGV(TAG, "kv_get_str malloc failed");
        return NULL;
    }

    // get actual string
    err = nvs_get_str(handle, key, buf, &len);
    ESP_LOGV(TAG, "nvs_get_str %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        free(buf);
        ESP_LOGV(TAG, "nvs_get_str error");
        return NULL;
    }

    ESP_LOGV(TAG, "kv_get_str ret=%s", buf);
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
    ESP_LOGV(TAG, "nvs_get_blob %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGV(TAG, "nvs_get_blob error");
        return NULL;
    }
    ESP_LOGV(TAG, "nvs_get_blob len=%u", buf_len);

    // handle empty blobs
    if (buf_len == 0)
    {
        char *tmp = malloc(1);
        tmp[0] = 0;
        return tmp;
    }

    // allocate memory
    buf = malloc(buf_len);
    assert(buf != NULL);
    if (buf == NULL) // if asserts are disabled
    {
        ESP_LOGW(TAG, "kv_get_blob malloc failed");
        return NULL;
    }

    // get actual string
    err = nvs_get_blob(handle, key, buf, &buf_len);
    ESP_LOGV(TAG, "nvs_get_blob %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        free(buf);
        ESP_LOGW(TAG, "nvs_get_blob error");
        return NULL;
    }

    *length = buf_len;
    ESP_LOGV(TAG, "kv_get_str len=%u ret=%p", buf_len, buf);
    return buf; // caller MUST FREE returned value if not NULL
}

void kv_ns_set_i8_atomic(const char *namespace, const char *key, int8_t value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    kv_set_i8(nvs_h, key, value);
    kv_commit(nvs_h);
    kv_close(nvs_h);
}

void kv_ns_set_u8_atomic(const char *namespace, const char *key, uint8_t value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    kv_set_u8(nvs_h, key, value);
    kv_commit(nvs_h);
    kv_close(nvs_h);
}

void kv_ns_set_i16_atomic(const char *namespace, const char *key, int16_t value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    kv_set_i16(nvs_h, key, value);
    kv_commit(nvs_h);
    kv_close(nvs_h);
}

void kv_ns_set_u16_atomic(const char *namespace, const char *key, uint16_t value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    kv_set_u16(nvs_h, key, value);
    kv_commit(nvs_h);
    kv_close(nvs_h);
}

void kv_ns_set_i32_atomic(const char *namespace, const char *key, int32_t value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    kv_set_i32(nvs_h, key, value);
    kv_commit(nvs_h);
    kv_close(nvs_h);
}

void kv_ns_set_u32_atomic(const char *namespace, const char *key, uint32_t value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    kv_set_u32(nvs_h, key, value);
    kv_commit(nvs_h);
    kv_close(nvs_h);
}

void kv_ns_set_i64_atomic(const char *namespace, const char *key, int64_t value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    kv_set_i64(nvs_h, key, value);
    kv_commit(nvs_h);
    kv_close(nvs_h);
}

void kv_ns_set_u64_atomic(const char *namespace, const char *key, uint64_t value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    kv_set_u64(nvs_h, key, value);
    kv_commit(nvs_h);
    kv_close(nvs_h);
}

void kv_ns_set_str_atomic(const char *namespace, const char *key, const char *value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    kv_set_str(nvs_h, key, value);
    kv_commit(nvs_h);
    kv_close(nvs_h);
}

void kv_ns_set_blob_atomic(const char *namespace, const char *key, const void *value, size_t length)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    kv_set_blob(nvs_h, key, value, length);
    kv_commit(nvs_h);
    kv_close(nvs_h);
}

int8_t kv_ns_get_i8_atomic(const char *namespace, const char *key, int8_t def_value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    int8_t value = kv_get_i8(nvs_h, key, def_value);
    kv_close(nvs_h);
    return value;
}

uint8_t kv_ns_get_u8_atomic(const char *namespace, const char *key, uint8_t def_value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    uint8_t value = kv_get_u8(nvs_h, key, def_value);
    kv_close(nvs_h);
    return value;
}

int16_t kv_ns_get_i16_atomic(const char *namespace, const char *key, int16_t def_value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    int16_t value = kv_get_i16(nvs_h, key, def_value);
    kv_close(nvs_h);
    return value;
}

uint16_t kv_ns_get_u16_atomic(const char *namespace, const char *key, uint16_t def_value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    uint16_t value = kv_get_u16(nvs_h, key, def_value);
    kv_close(nvs_h);
    return value;
}

int32_t kv_ns_get_i32_atomic(const char *namespace, const char *key, int32_t def_value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    int32_t value = kv_get_i32(nvs_h, key, def_value);
    kv_close(nvs_h);
    return value;
}

uint32_t kv_ns_get_u32_atomic(const char *namespace, const char *key, uint32_t def_value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    uint32_t value = kv_get_u32(nvs_h, key, def_value);
    kv_close(nvs_h);
    return value;
}

int64_t kv_ns_get_i64_atomic(const char *namespace, const char *key, int64_t def_value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    int64_t value = kv_get_i64(nvs_h, key, def_value);
    kv_close(nvs_h);
    return value;
}

uint64_t kv_ns_get_u64_atomic(const char *namespace, const char *key, uint64_t def_value)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    uint64_t value = kv_get_u64(nvs_h, key, def_value);
    kv_close(nvs_h);
    return value;
}

/* Returned memory MUST BE FREED (if not NULL) BY THE CALLER  */
char *kv_ns_get_str_atomic(const char *namespace, const char *key)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    char *value = kv_get_str(nvs_h, key);
    kv_close(nvs_h);
    return value;
}

/* Returned memory MUST BE FREED (if not NULL) BY THE CALLER  */
void *kv_ns_get_blob_atomic(const char *namespace, const char *key, size_t *length)
{
    nvs_handle_t nvs_h;
    nvs_h = kv_open_ns(namespace);
    void *value = kv_get_blob(nvs_h, key, length);
    kv_close(nvs_h);
    return value;
}
