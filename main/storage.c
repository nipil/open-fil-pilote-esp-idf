
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

void test_storage(void)
{
    part_list();
    kv_init();
    kv_stats();
    kv_list_ns(NULL);

}