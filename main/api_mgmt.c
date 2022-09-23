#include <cjson.h>
#include <esp_log.h>
#include <esp_ota_ops.h>

#include "str.h"
#include "ofp.h"
#include "webserver.h"
#include "uptime.h"
#include "api_mgmt.h"
#include "fwupd.h"

static const char TAG[] = "api_mgmt";

#define REBOOT_WAIT_SEC 10
#define CERT_BUNDLE_MAX_LENGTH (16 * 1024)

/***************************************************************************/

static void reboot_wait(void *pvParameters)
{
    ESP_LOGI(TAG, "Rebooting in %i seconds...", REBOOT_WAIT_SEC);
    wait_sec(REBOOT_WAIT_SEC);

    ESP_LOGI(TAG, "Rebooting NOW !");
    // Task should NOT exit (or BY DEFAULT it causes FreeRTOS to abort()
    esp_restart();
}

static void mgmt_queue_reboot(void)
{
    // notify the webserver to not serve anything anymore
    webserver_disable();

    // start task for delayed reboot
    TaskHandle_t xHandle = NULL;
    xTaskCreatePinnedToCore(reboot_wait, "reboot_wait", 2048, NULL, 1, &xHandle, 1);
    configASSERT(xHandle);
}

/***************************************************************************/

esp_err_t serve_api_get_status(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_status version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    time_t sys_uptime = get_system_uptime();
    const struct uptime_wifi *wi = uptime_get_wifi_stats();

    time_t current_wifi_uptime;
    time(&current_wifi_uptime);
    current_wifi_uptime -= wi->last_connect_time;

    cJSON *root = cJSON_CreateObject();

    // system uptime
    cJSON *uptime = cJSON_AddObjectToObject(root, "uptime");
    cJSON_AddNumberToObject(uptime, "system", sys_uptime);

    // wifi uptime and statistics
    cJSON *wifi = cJSON_AddObjectToObject(uptime, "wifi");
    cJSON_AddNumberToObject(wifi, "attempts", wi->attempts);
    cJSON_AddNumberToObject(wifi, "successes", wi->successes);
    cJSON_AddNumberToObject(wifi, "disconnects", wi->disconnects);
    cJSON_AddNumberToObject(wifi, "cumulated_uptime", wi->cumulated_uptime + current_wifi_uptime);
    cJSON_AddNumberToObject(wifi, "current_uptime", current_wifi_uptime);

    // firmware and OTA versions and dates
    cJSON *ota = cJSON_AddObjectToObject(root, "firmware");
    const esp_partition_t *part_running = esp_ota_get_running_partition();
    cJSON_AddStringToObject(ota, "running_partition", part_running->label);
    cJSON_AddNumberToObject(ota, "running_partition_size", part_running->size);
    const esp_app_desc_t *ead = esp_ota_get_app_description();
    cJSON_AddStringToObject(ota, "running_app_name", ead->project_name);
    cJSON_AddStringToObject(ota, "running_app_version", ead->version);
    cJSON_AddStringToObject(ota, "running_app_compiled_date", ead->date);
    cJSON_AddStringToObject(ota, "running_app_compiled_time", ead->time);
    cJSON_AddStringToObject(ota, "running_app_idf_version", ead->idf_ver);

    // user infos
    cJSON *user = cJSON_AddObjectToObject(root, "user");
    bool user_is_admin = ofp_session_user_is_admin(req);
    cJSON_AddBoolToObject(user, admin_str, user_is_admin);
    char *user_id = ofp_session_get_user(req);
    if (user_id)
        cJSON_AddStringToObject(user, json_key_id, user_id);
    else
        cJSON_AddNullToObject(user, json_key_id);
    char *source_ip = ofp_session_get_source_ip_address(req);
    if (source_ip)
        cJSON_AddStringToObject(user, json_key_source_ip, source_ip);
    else
        cJSON_AddNullToObject(user, json_key_source_ip);

    esp_err_t result = serve_json(req, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t serve_api_get_reboot(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_reboot version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    // restrict access
    if (!ofp_session_user_is_admin(req))
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Unauthorized");

    // dump some dev stats before rebooting
    uint32_t min = esp_get_minimum_free_heap_size();
    ESP_LOGI(TAG, "Minimum heap that has ever been available: %u", min);

    // prepare restart, giving it the time to serve the page and cleanup
    mgmt_queue_reboot();

    // serve wait page
    return serve_static_ofp_wait_html(req);
}

/*
 * OTA driver for contiguous network data
 */
static esp_err_t serve_api_post_upgrade_raw(httpd_req_t *req)
{
    ESP_LOGD(TAG, "serve_api_post_upgrade_raw");

    static char buf[CONFIG_OFP_UI_WEBSERVER_DATA_MAX_SIZE_SINGLE_OP];

    int remaining = req->content_len;
    int len = 0;

    struct fwupd_data upd;
    esp_err_t result = fwupd_begin(&upd);
    ESP_LOGD(TAG, "fwupd_begin %i", result);
    if (result != ESP_OK)
        goto cleanup;

    while (remaining != 0)
    {
        // limit to buffer size
        len = min_int(remaining, sizeof(buf));

        // get from network
        result = webserver_read_request_data(req, buf, len);
        ESP_LOGD(TAG, "webserver_read_request_data %i", result);
        if (result != ESP_OK)
            goto cleanup;

        // count
        remaining -= len;
        ESP_LOG_BUFFER_HEXDUMP(TAG, buf, len, ESP_LOG_VERBOSE);

        // put to flash
        result = fwupd_write(&upd, buf, len);
        ESP_LOGD(TAG, "fwupd_write %i", result);
        if (result != ESP_OK)
            goto cleanup;
    }

    result = fwupd_end(&upd);
    ESP_LOGD(TAG, "fwupd_end %i", result);
    if (result != ESP_OK)
        goto cleanup;

    return ESP_OK;

cleanup:
    return result;
}

/*
 * Push a new firmware to the device
 *
 * Sample upload with curl:
 * curl --silent --show-error --header "Content-Type: application/octet-stream" -X POST --insecure https://admin:admin@openfilpilote.local/ofp-api/v1/upgrade --data-binary @ota.bin
 *
 * Firmware will try to be used on next reboot, which does NOT happen automatically and must be performed by the user (either through API call or button press)
 */
esp_err_t serve_api_post_upgrade(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_post_upgrade version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    // restrict access
    if (!ofp_session_user_is_admin(req))
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Unauthorized");

    ESP_LOGD(TAG, "Content-length: %u", req->content_len);

    // cleanup variables
    char *content_type_hdr_value = ofp_webserver_get_header_string(req, http_content_type_hdr);
    if (content_type_hdr_value == NULL)
    {
        ESP_LOGW(TAG, "Could not get value for header: %s", content_type_hdr_value);
        goto cleanup;
    }

    ESP_LOGD(TAG, "content_type_hdr_value: %s", content_type_hdr_value);
    if (strcmp(content_type_hdr_value, str_application_octet_stream) == 0)
    {
        ESP_LOGD(TAG, "Attempt CURL-like OTA upload");
        esp_err_t err = serve_api_post_upgrade_raw(req);
        ESP_LOGV(TAG, "serve_api_post_upgrade_raw: %i", err);
        if (err != ESP_OK)
            goto cleanup;

        // success
        ESP_LOGI(TAG, "Firmware installation finished successfully");
        free(content_type_hdr_value);

        return httpd_resp_sendstr(req, "firmware update successful");
    }

    free(content_type_hdr_value);
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");

cleanup:
    free(content_type_hdr_value);
    return httpd_resp_send_500(req);
}
/*
 * Upload a PEM certificate bundle (with an unencrypted key)
 *
 * Example CLI usage :
 * curl --silent --show-error --header "Content-Type: application/x-pem-file" -X POST --insecure https://admin:admin@ipaddress/ofp-api/v1/certificate --data-ascii @bundle.pem
 *
 * Certificate will try to be used on next reboot, which does NOT happen automatically and must be performed by the user (either through API call or button press)
 */
esp_err_t serve_api_post_certificate(httpd_req_t *req, struct re_result *captures)
{
    esp_log_level_set(TAG, ESP_LOG_VERBOSE); // DEBUG
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_post_upgrade version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    // restrict access
    if (!ofp_session_user_is_admin(req))
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Unauthorized");

    // cleanup variables
    char *buf = NULL;
    struct certificate_bundle_iter *it = NULL;

    // verify header
    buf = ofp_webserver_get_header_string(req, http_content_type_hdr);
    if (buf == NULL)
    {
        ESP_LOGW(TAG, "Could not get value for header: %s", buf);
        goto cleanup;
    }

    ESP_LOGD(TAG, "content_type_hdr_value: %s", buf);
    if (strcmp(buf, str_application_x_pem_file) != 0)
    {
        ESP_LOGW(TAG, "Invalid content type %s, expected %s", buf, str_application_x_pem_file);
        goto cleanup;
    }

    // cleanup
    free(buf);
    buf = NULL;

    // large atomic
    ESP_LOGD(TAG, "Content-length: %u", req->content_len);
    buf = webserver_get_request_data_atomic_max_size(req, CERT_BUNDLE_MAX_LENGTH);
    if (buf == NULL)
    {
        ESP_LOGW(TAG, "Provided data is %i bytes and too large to handle, maximum allowed size is %i", req->content_len, CERT_BUNDLE_MAX_LENGTH - 1);
        goto cleanup;
    }
    // ESP_LOG_BUFFER_HEXDUMP(TAG, buf, req->content_len, ESP_LOG_VERBOSE);

    // parse
    it = certificate_bundle_iter_init(buf, req->content_len);
    if (it == NULL)
        goto cleanup;

    while (certificate_bundle_iter_next(it))
    {
        switch (it->state)
        {
        case CBIS_CERTIFICATE:
            ESP_LOGD(TAG, "CERTIFICATE from %p length %i", it->block_start, it->block_len);
            ESP_LOG_BUFFER_HEXDUMP(TAG, it->block_start, it->block_len, ESP_LOG_VERBOSE);
            break;
        case CBIS_PRIVATE_KEY:
            ESP_LOGD(TAG, "PRIVATE KEY from %p length %i", it->block_start, it->block_len);
            ESP_LOG_BUFFER_HEXDUMP(TAG, it->block_start, it->block_len, ESP_LOG_VERBOSE);
            break;
        default:
            ESP_LOGE(TAG, "incorrect CBIS state while returning success");
            break;
        }
    }
    switch (it->state)
    {
    case CBIS_END_OK:
        ESP_LOGI(TAG, "Certificate bundle iterated successfully");
        break;
    case CBIS_END_FAIL:
        ESP_LOGW(TAG, "Problem occured while iterating over certificate bundle");
        break;
    default:
        ESP_LOGE(TAG, "incorrect CBIS state while returning success");
        break;
    }

cleanup:
    certificate_bundle_iter_free(it);
    free(buf);
    return httpd_resp_send_500(req);
}
