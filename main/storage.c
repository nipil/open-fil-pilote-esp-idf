
#include <stdio.h>
#include <esp_log.h>
#include <esp_check.h>
#include <esp_partition.h>
#include <nvs_flash.h>
#include <nvs.h>

static const char TAG[] = "storage";

const char wifi_manager_nvs_namespace[] = "espwifimgr";

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

void kv_erase(void)
{
    ESP_LOGW(TAG, "Erasing flash... Every settings will be deleted.");
    esp_err_t err = nvs_flash_erase();
    ESP_LOGD(TAG, "nvs_flash_erase: %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_stats(void)
{
    nvs_stats_t nvs_stats;
    esp_err_t err = nvs_get_stats(NULL, &nvs_stats);
    ESP_LOGD(TAG, "nvs_get_stats: %s", esp_err_to_name(err));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Storage stats failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "NVS stats: Used %d, Free = (%d), All = (%d)",
             nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);
}

void kv_init(void)
{
    esp_err_t err = nvs_flash_init();
    ESP_LOGD(TAG, "nvs_flash_init: %s", esp_err_to_name(err));
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGI(TAG, "Mounting error: %s", esp_err_to_name(err));
        // NVS partition was truncated and needs to be erased
        kv_erase();
        // Retry nvs_flash_init
        ESP_LOGI(TAG, "Retrying mounting flash...");
        err = nvs_flash_init();
        ESP_LOGD(TAG, "nvs_flash_init: %s", esp_err_to_name(err));
    }
    ESP_ERROR_CHECK(err);
}

void kv_list_ns(const char *namespace)
{
    nvs_iterator_t it = nvs_entry_find("nvs", namespace, NVS_TYPE_ANY);
    while (it != NULL)
    {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        ESP_LOGD(TAG, "Namespace '%s', key '%s', type '%d'", info.namespace_name, info.key, info.type);
        it = nvs_entry_next(it);
    }
    nvs_release_iterator(it);
}

nvs_handle_t kv_open_ns(const char *namespace)
{
    ESP_LOGD(TAG, "Open namespace %s", (namespace != NULL) ? namespace : "NULL");
    assert(namespace != NULL);
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    ESP_LOGD(TAG, "nvs_open: %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
    return handle;
}

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
    ESP_LOGD(TAG, "kv_set_str key=%s value=%s", key, (value != NULL) ? value : "NULL");
    assert(value != NULL);
    esp_err_t err = nvs_set_str(handle, key, value);
    ESP_LOGD(TAG, "nvs_set_str %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void kv_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length)
{
    ESP_LOGD(TAG, "kv_set_i8 key=%s value=%p length=%u", key, value, length);
    assert(value != NULL);
    esp_err_t err = nvs_set_blob(handle, key, value, length);
    ESP_LOGD(TAG, "nvs_set_blob %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
}

void test_storage(void)
{
    part_list();
    kv_init();
    kv_stats();
    kv_list_ns(NULL);

    esp_err_t err;

    nvs_handle_t h = kv_open_ns("test");
    kv_set_i8(h, "i8", INT8_MIN);
    kv_set_u8(h, "u8", UINT8_MAX);
    kv_set_i16(h, "i16", INT16_MIN);
    kv_set_u16(h, "u16", UINT16_MAX);
    kv_set_i32(h, "i32", INT32_MIN);
    kv_set_u32(h, "u32", UINT32_MAX);
    kv_set_i64(h, "i64", INT64_MIN);
    kv_set_u64(h, "u64", UINT64_MAX);
    kv_set_str(h, "str", "foo");
    uint8_t blob[] = {0x69, 0x51};
    kv_set_blob(h, "blob", blob, sizeof(blob));

    err = nvs_commit(h);
    ESP_ERROR_CHECK(err);

    nvs_close(h);
}