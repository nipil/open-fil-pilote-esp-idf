#include <string.h>
#include <esp_log.h>

#include "fwupd.h"

static const char TAG[] = "fwupd";

void fwupd_log_part_info(void)
{
    const esp_partition_t *part_b = esp_ota_get_boot_partition();
    ESP_LOGI(TAG, "boot partition: label %s addr %08X size %u", part_b->label, part_b->address, part_b->size);
    const esp_partition_t *part_running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "running partition: label %s addr %08X size %u", part_running->label, part_running->address, part_running->size);
    const esp_partition_t *part_next = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "next update partition: label %s addr %08X size %u", part_next->label, part_next->address, part_next->size);
}

/*
 * wrapper to start an OTA update
 */
esp_err_t fwupd_begin(struct fwupd_data *data)
{
    esp_err_t result = ESP_FAIL;

    if (data == NULL)
        return ESP_ERR_NO_MEM;

    memset(data, 0, sizeof(struct fwupd_data));
    data->headers_checked = false;
    data->update_handle = 0;
    data->next_partition = esp_ota_get_next_update_partition(NULL);
    if (data->next_partition == NULL)
    {
        ESP_LOGW(TAG, "No available partition for OTA update");
        goto cleanup;
    }

    ESP_LOGD(TAG, "Writing to partition %s of subtype %d at offset 0x%x", data->next_partition->label, data->next_partition->subtype, data->next_partition->address);
    result = esp_ota_begin(data->next_partition, OTA_WITH_SEQUENTIAL_WRITES, &data->update_handle);
    ESP_LOGV(TAG, "esp_ota_begin %i", result);
    if (result != ESP_OK)
    {
        ESP_LOGW(TAG, "Could not start OTA update (%s)", esp_err_to_name(result));
        goto cleanup;
    }

    return ESP_OK;

cleanup:
    return result;
}

/*
 * wrapper to write an OTA chunk
 */
esp_err_t fwupd_write(struct fwupd_data *data, const void *buf, size_t len)
{
    if (data == NULL)
        return ESP_ERR_NO_MEM;

    esp_err_t result = ESP_FAIL;

    if (buf == NULL || len == 0)
        goto cleanup;

    // check
    if (!data->headers_checked && len >= sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
    {
        data->headers_checked = true;

        // image magic
        if (len >= sizeof(esp_image_header_t))
        {
            esp_image_header_t *eih = (esp_image_header_t *)buf;
            if (eih->magic != ESP_IMAGE_HEADER_MAGIC)
            {
                ESP_LOGW(TAG, "OTA image header magic mismatch: expected 0x%02x and got 0x%02x", ESP_IMAGE_HEADER_MAGIC, eih->magic);
                goto cleanup;
            }
        }

        // app magic
        if (len >= sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
        {
            const esp_app_desc_t *ead = (const esp_app_desc_t *)((uint8_t *)buf + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t));
            if (ead->magic_word != ESP_APP_DESC_MAGIC_WORD)
            {
                ESP_LOGW(TAG, "OTA app info header magic mismatch: expected 0x%08x and got 0x%08x", ESP_APP_DESC_MAGIC_WORD, ead->magic_word);
                goto cleanup;
            }
            ESP_LOGI(TAG, "New firmware version: %s, date %s time %s", ead->version, ead->date, ead->time);
        }
    }

    // write
    result = esp_ota_write(data->update_handle, buf, len);
    ESP_LOGV(TAG, "esp_ota_write %i", result);
    if (result != ESP_OK)
    {
        ESP_LOGW(TAG, "Could not write chunck of OTA update (%s)", esp_err_to_name(result));
        goto cleanup;
    }

    return ESP_OK;

cleanup:
    esp_ota_abort(data->update_handle);
    return result;
}

/*
 * wrapper to finish an OTA update
 */
esp_err_t fwupd_end(struct fwupd_data *data)
{
    if (data == NULL)
        return ESP_ERR_NO_MEM;

    esp_err_t result = esp_ota_end(data->update_handle);
    ESP_LOGV(TAG, "esp_ota_end %i", result);
    if (result != ESP_OK)
    {
        if (result == ESP_ERR_OTA_VALIDATE_FAILED)
        {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        }
        else
        {
            ESP_LOGE(TAG, "Could not end OTA update properly (%s)!", esp_err_to_name(result));
        }
        goto cleanup;
    }

    ESP_LOGD(TAG, "esp_ota_end finished");

    /*
        I (4455) esp_image: segment 0: paddr=00150020 vaddr=3f400020 size=08648h ( 34376) map
        I (4465) esp_image: segment 1: paddr=00158670 vaddr=3ffb0000 size=02414h (  9236)
        I (4475) esp_image: segment 2: paddr=0015aa8c vaddr=40080000 size=0558ch ( 21900)
        I (4485) esp_image: segment 3: paddr=00160020 vaddr=400d0020 size=15608h ( 87560) map
        I (4515) esp_image: segment 4: paddr=00175630 vaddr=4008558c size=05each ( 24236)
        I (4525) esp_image: segment 5: paddr=0017b4e4 vaddr=50000000 size=00010h (    16)
    */

    result = esp_ota_set_boot_partition(data->next_partition);
    ESP_LOGV(TAG, "esp_ota_set_boot_partition %i", result);
    if (result != ESP_OK)
    {
        ESP_LOGW(TAG, "Could not set updated partition as boot partition (%s)!", esp_err_to_name(result));
        goto cleanup;
    }

    return ESP_OK;

cleanup:
    esp_ota_abort(data->update_handle);
    return result;
}
