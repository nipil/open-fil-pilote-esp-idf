#include <string.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

#include "fwupd.h"

static const char TAG[] = "fwupd";

void ota_check(void)
{
    // DEBUG
    ESP_LOGI(TAG, "OTA partition count %i", esp_ota_get_app_partition_count());
    const esp_app_desc_t *ead = esp_ota_get_app_description();
    ESP_LOGI(TAG, "magic_word %u secure_version %u version %s project_name %s date %s time %s idf_ver %s", ead->magic_word, ead->secure_version, ead->version, ead->project_name, ead->time, ead->date, ead->idf_ver);

    char h[65];
    int r = esp_ota_get_app_elf_sha256(h, 65);
    assert(r - 1 == CONFIG_APP_RETRIEVE_LEN_ELF_SHA);
    ESP_LOGI(TAG, "Begining of image sha256 in memory: %s", h);

    const esp_partition_t *part_b = esp_ota_get_boot_partition();
    ESP_LOGI(TAG, "boot partition: label %s addr %08X size %u", part_b->label, part_b->address, part_b->size);

    const esp_partition_t *part_running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "running partition: label %s addr %08X size %u", part_running->label, part_running->address, part_running->size);

    const esp_partition_t *part_next = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "next update partition: label %s addr %08X size %u", part_next->label, part_next->address, part_next->size);
}

void ota_try()
{
    const esp_partition_t *part_running = esp_ota_get_running_partition();
    const esp_partition_t *part_next = esp_ota_get_next_update_partition(NULL);

    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0;

    assert(part_next != NULL);

    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x", part_next->subtype, part_next->address);

    extern const unsigned char ota_bin_start[] asm("_binary_ota_bin_start");
    extern const unsigned char ota_bin_end[] asm("_binary_ota_bin_end");
    int ota_bin_length = ota_bin_end - ota_bin_start;
    ESP_LOGI(TAG, "ota.bin start %p end %p length %u", ota_bin_start, ota_bin_end, ota_bin_length);

#define BUFFSIZE 1024
    static char ota_write_data[BUFFSIZE + 1] = {0};
    int data_read = 0;
    unsigned char *current = (unsigned char *)ota_bin_start;
    const unsigned char *end = ota_bin_end;

    esp_err_t err = esp_ota_begin(part_next, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
    ESP_LOGI(TAG, "esp_ota_begin %i", err);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        goto cleanup;
    }
    ESP_LOGI(TAG, "esp_ota_begin succeeded");

    bool first = true;

    int total = 0;
    // first read
    ESP_LOGI(TAG, "min analysis size %u", sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t));
    while (current != end)
    {
        data_read = end - current;
        if (data_read > BUFFSIZE)
            data_read = BUFFSIZE;
        memcpy(ota_write_data, current, data_read);
        current += data_read;

        err = ESP_OK;
        esp_app_desc_t new_app_info;

        if (first && data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
        {
            first = false;

            esp_image_header_t *eih = (esp_image_header_t *)ota_write_data;
            if (eih->magic != ESP_IMAGE_HEADER_MAGIC)
            {
                ESP_LOGW(TAG, "image header magic mismatch: expected 0x%02x and got 0x%02x", ESP_IMAGE_HEADER_MAGIC, eih->magic);
                // goto cleanup;
            }

            // check current version with downloading
            memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));

            if (new_app_info.magic_word != ESP_APP_DESC_MAGIC_WORD)
            {
                // for the hello world on github, the magic is 0x00000000
                ESP_LOGW(TAG, "app info header magic mismatch: expected 0x%08x and got 0x%08x", ESP_APP_DESC_MAGIC_WORD, new_app_info.magic_word);
                // goto cleanup;
            }

            ESP_LOGI(TAG, "New firmware version: %s, date %s time %s", new_app_info.version, new_app_info.date, new_app_info.time);

            esp_app_desc_t running_app_info;
            if (esp_ota_get_partition_description(part_running, &running_app_info) == ESP_OK)
            {
                ESP_LOGI(TAG, "Running firmware version: %s, date %s time %s", running_app_info.version, running_app_info.date, running_app_info.time);
            }

            const esp_partition_t *last_invalid_app = esp_ota_get_last_invalid_partition();
            esp_app_desc_t invalid_app_info;
            if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK)
            {
                ESP_LOGI(TAG, "Last invalid firmware version: %s, date %s time %s", invalid_app_info.version, invalid_app_info.date, invalid_app_info.time);
            }
        }

        ESP_LOGV(TAG, "--- ota_bin_length %u written %u to_write %u", ota_bin_length, total, data_read);
        // ESP_LOG_BUFFER_HEXDUMP(TAG, ota_write_data, data_read, ESP_LOG_VERBOSE);
        err = esp_ota_write(update_handle, (const void *)ota_write_data, data_read);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
            goto cleanup;
        }

        total += data_read;
    }

    ESP_LOGV(TAG, "--- ota_bin_length %u written %u", ota_bin_length, total);

    ESP_LOGI(TAG, "esp_ota_writes finished");

    err = esp_ota_end(update_handle);
    ESP_LOGI(TAG, "esp_ota_end %i", err);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED)
        {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        }
        else
        {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }
        goto cleanup;
    }

    ESP_LOGI(TAG, "esp_ota_end finished");

    err = esp_ota_set_boot_partition(part_next);
    ESP_LOGI(TAG, "esp_ota_set_boot_partition %i", err);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        goto cleanup;
    }

    ESP_LOGI(TAG, "Prepare to restart system!");
    esp_restart();
    return;

    /*

    doing OTA
    I (415) fwupd: OTA partition count 2
    I (415) fwupd: magic_word 2882360370 secure_version 0 version cf80c39-dirty project_name open-fil-pilote-esp-idf date 17:44:42 time Sep 21 2022 idf_ver v4.4.2
    I (435) fwupd: Begining of image sha256 in memory: 46e18692cf46b511
    I (445) fwupd: boot partition: label factory addr 00010000 size 1310720
    I (445) fwupd: running partition: label factory addr 00010000 size 1310720
    I (455) fwupd: next update partition: label ota_0 addr 00150000 size 1310720

    I (465) fwupd: Writing to partition subtype 16 at offset 0x150000
    I (475) fwupd: ota.bin start 0x3f403732 end 0x3f42ec52 length 177440
    I (475) fwupd: read 1024 current 0x3f403b32 end 0x3f42ec52
    I (485) fwupd: min analysis size 288
    I (485) fwupd: New firmware version: fc75dc4-dirty, date Sep 21 2022 time 16:48:37
    I (495) fwupd: Running firmware version: cf80c39-dirty, date Sep 21 2022 time 17:44:42
    I (505) fwupd: esp_ota_begin 0
    I (505) fwupd: esp_ota_begin succeeded
    I (4455) fwupd: esp_ota_writes finished
    I (4455) esp_image: segment 0: paddr=00150020 vaddr=3f400020 size=08648h ( 34376) map
    I (4465) esp_image: segment 1: paddr=00158670 vaddr=3ffb0000 size=02414h (  9236)
    I (4475) esp_image: segment 2: paddr=0015aa8c vaddr=40080000 size=0558ch ( 21900)
    I (4485) esp_image: segment 3: paddr=00160020 vaddr=400d0020 size=15608h ( 87560) map
    I (4515) esp_image: segment 4: paddr=00175630 vaddr=4008558c size=05each ( 24236)
    I (4525) esp_image: segment 5: paddr=0017b4e4 vaddr=50000000 size=00010h (    16)
    I (4525) fwupd: esp_ota_end 0
    I (4525) fwupd: esp_ota_end finished
    I (4525) esp_image: segment 0: paddr=00150020 vaddr=3f400020 size=08648h ( 34376) map
    I (4545) esp_image: segment 1: paddr=00158670 vaddr=3ffb0000 size=02414h (  9236)
    I (4555) esp_image: segment 2: paddr=0015aa8c vaddr=40080000 size=0558ch ( 21900)
    I (4565) esp_image: segment 3: paddr=00160020 vaddr=400d0020 size=15608h ( 87560) map
    I (4595) esp_image: segment 4: paddr=00175630 vaddr=4008558c size=05each ( 24236)
    I (4605) esp_image: segment 5: paddr=0017b4e4 vaddr=50000000 size=00010h (    16)
    I (4645) fwupd: esp_ota_set_boot_partition 0
    I (4645) fwupd: Prepare to restart system!

    after reboot
    I (1282) fwupd: OTA partition count 2
    I (1312) fwupd: magic_word 2882360370 secure_version 0 version fc75dc4-dirty project_name open-fil-pilote-esp-idf date 16:48:37 time Sep 21 2022 idf_ver v4.4.2
    I (1332) fwupd: Begining of image sha256 in memory: a064f006305841bb

    I (1352) fwupd: boot partition: label ota_0 addr 00150000 size 1310720
    I (1352) fwupd: running partition: label ota_0 addr 00150000 size 1310720
    I (1362) fwupd: next update partition: label ota_1 addr 00290000 size 1310720
    */

cleanup:
    // http_cleanup(client);
    esp_ota_abort(update_handle);
    // task_fatal_error();
}
