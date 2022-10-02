#include <cjson.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_ota_ops.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <mbedtls/error.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>

#include "str.h"
#include "ofp.h"
#include "webserver.h"
#include "uptime.h"
#include "api_mgmt.h"
#include "fwupd.h"
#include "storage.h"

static const char TAG[] = "api_mgmt";

#define REBOOT_WAIT_SEC 10
#define CERT_BUNDLE_MAX_LENGTH (16 * 1024)
#define CERT_BUNDLE_MAX_PART_COUNT 8

#define SELF_SIGNED_CERT_RSA_KEY_SIZE 2048
#define SELF_SIGNED_CERT_OUTPUT_PEM_MAX_LENGTH SELF_SIGNED_CERT_RSA_KEY_SIZE
#define SELF_SIGNED_CERT_SERIAL_NUMBER "1"
#define SELF_SIGNED_CERT_NOT_BEFORE "20200101000000"
#define SELF_SIGNED_CERT_NOT_AFTER "20401231235959"

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

    // Log partition status before the update
    fwupd_log_part_info();

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

    // Log partition status after the update
    fwupd_log_part_info();

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

esp_err_t serve_api_get_certificate(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_certificate version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    // restrict access
    if (!ofp_session_user_is_admin(req))
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Unauthorized");

    cJSON *root = cJSON_CreateObject();

    // embedded certs and key
    cJSON *embedded = cJSON_AddObjectToObject(root, "embedded");
    extern const unsigned char cacert_pem_start[] asm("_binary_autosign_crt_start");
    cJSON_AddStringToObject(embedded, stor_key_https_certs, (char *)cacert_pem_start);
    extern const unsigned char prvtkey_pem_start[] asm("_binary_autosign_key_start");
    cJSON_AddStringToObject(embedded, stor_key_https_key, (char *)prvtkey_pem_start);

    // stored certs and key
    char *https_certs = kv_ns_get_str_atomic(kv_get_ns_ofp(), stor_key_https_certs);
    cJSON *stored = cJSON_AddObjectToObject(root, "stored");
    if (https_certs != NULL)
        cJSON_AddStringToObject(stored, stor_key_https_certs, https_certs);
    else
        cJSON_AddNullToObject(stored, stor_key_https_certs);
    char *https_key = kv_ns_get_str_atomic(kv_get_ns_ofp(), stor_key_https_key);
    if (https_key != NULL)
        cJSON_AddStringToObject(stored, stor_key_https_key, https_key);
    else
        cJSON_AddNullToObject(stored, stor_key_https_key);

    esp_err_t result = serve_json(req, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t serve_api_delete_certificate(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_delete_certificate version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    // restrict access
    if (!ofp_session_user_is_admin(req))
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Unauthorized");

    kv_ns_delete_atomic(kv_get_ns_ofp(), stor_key_https_certs);
    kv_ns_delete_atomic(kv_get_ns_ofp(), stor_key_https_key);

    return httpd_resp_sendstr(req, "Stored cert and key removed, only firmware-embedded cert and key remains. Please reboot.");
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

    // result
    int result = 0;
    char *msg = NULL;

    // matedata structure for certificates
    struct data
    {
        char *pem_start;
        size_t pem_len;
        void *data;
        bool used;
    };

    // cleanup variables
    char *buf = NULL;
    struct certificate_bundle_iter *it = NULL;
    mbedtls_x509_crt *leaf_cert = NULL;
    int cert_count = 0;
    int pk_count = 0;
    struct data certs[CERT_BUNDLE_MAX_PART_COUNT] = {0};
    struct data pks[CERT_BUNDLE_MAX_PART_COUNT] = {0};

    // verify header
    buf = ofp_webserver_get_header_string(req, http_content_type_hdr);
    if (buf == NULL)
    {
        ESP_LOGW(TAG, "Could not get value for header: %s", buf);
        result = HTTPD_500_INTERNAL_SERVER_ERROR;
        goto cleanup;
    }

    ESP_LOGV(TAG, "content_type_hdr_value: %s", buf);
    if (strcmp(buf, str_application_x_pem_file) != 0)
    {
        ESP_LOGW(TAG, "Invalid content type %s, expected %s", buf, str_application_x_pem_file);
        result = HTTPD_400_BAD_REQUEST;
        msg = "Invalid content type";
        goto cleanup;
    }

    // clean for reuse
    free(buf);
    buf = NULL;

    // large atomic
    ESP_LOGD(TAG, "Content-length: %u", req->content_len);
    buf = webserver_get_request_data_atomic_max_size(req, CERT_BUNDLE_MAX_LENGTH);
    if (buf == NULL)
    {
        ESP_LOGW(TAG, "Provided data is %i bytes and too large to handle, maximum allowed size is %i", req->content_len, CERT_BUNDLE_MAX_LENGTH - 1);
        result = HTTPD_400_BAD_REQUEST;
        msg = "Input data too large to process";
        goto cleanup;
    }

    // parse
    it = certificate_bundle_iter_init(buf, req->content_len);
    if (it == NULL)
    {
        result = HTTPD_500_INTERNAL_SERVER_ERROR;
        goto cleanup;
    }

    int iteration = 0;
    while (certificate_bundle_iter_next(it))
    {
        iteration++;
        certificate_bundle_iter_log(it, TAG, ESP_LOG_DEBUG);

        // The parsing function is made to parse multiple certificates in the same string
        // its doc explicitely states that the string length includes null terminatin char.
        // Here i want to parse certificates ONE BY ONE so i must modify the buffer on the
        // fly to add the null characters, so that mbedtls parsing stops after each.
        //
        // And we do not overflow even if there is no terminating null byte on the input
        // as webserver_get_request_data_atomic_* added a \0 terminating byte as safeguard
        // which we overwrite here.
        //
        // So we keep the original character, override it, process, then restore it.
        char keep = it->block_start[it->block_len];
        it->block_start[it->block_len] = 0;
        it->block_len++;

        // cert found
        if (it->state == CBIS_CERTIFICATE && cert_count < CERT_BUNDLE_MAX_PART_COUNT)
        {
            mbedtls_x509_crt *crt = malloc(sizeof(mbedtls_x509_crt));
            if (crt == NULL)
                goto cleanup;
            result = pem_parse_single_certificate(it->block_start, it->block_len, crt);
            ESP_LOGV(TAG, "pem_parse_single_certificate %i", result);
            if (result != 0)
            {
                free(crt);
                ESP_LOGW(TAG, "Certificate parsing error 0x%04X", -result);
                ESP_LOGD(TAG, "%.*s", it->block_len, it->block_start);
                result = HTTPD_400_BAD_REQUEST;
                msg = "Certificate parsing error";
                goto cleanup;
            }
            ESP_LOGI(TAG, "Part %i parsed successfully as a certificate", iteration);
            certs[cert_count].data = crt;
            certs[cert_count].pem_start = it->block_start;
            certs[cert_count].pem_len = it->block_len;
            certs[cert_count].used = false;
            cert_count++;
        }

        // key found
        if (it->state == CBIS_PRIVATE_KEY && pk_count < CERT_BUNDLE_MAX_PART_COUNT)
        {
            mbedtls_pk_context *ctx = malloc(sizeof(mbedtls_pk_context));
            if (ctx == NULL)
                goto cleanup;
            result = pem_parse_single_private_key(it->block_start, it->block_len, NULL, 0, ctx);
            ESP_LOGV(TAG, "pem_parse_single_private_key %i", result);
            if (result != 0)
            {
                free(ctx);
                ESP_LOGW(TAG, "Private key parsing error:  0x%04X", -result);
                ESP_LOGD(TAG, "%.*s", it->block_len, it->block_start);
                result = HTTPD_400_BAD_REQUEST;
                msg = "Private key parsing error";
                goto cleanup;
            }
            ESP_LOGI(TAG, "Part %i parsed successfully as a private key", iteration);
            pks[pk_count].data = ctx;
            pks[pk_count].pem_start = it->block_start;
            pks[pk_count].pem_len = it->block_len;
            pk_count++;
        }

        // restore the original character (accounting for the fact that length was increased)
        it->block_start[it->block_len - 1] = keep;
    }

    // cleanup bundle iterator
    ESP_LOGD(TAG, "bundle finished");
    certificate_bundle_iter_log(it, TAG, ESP_LOG_DEBUG);
    if (it->state == CBIS_END_FAIL)
    {
        ESP_LOGW(TAG, "An error happened while on bundle %i, canceling", iteration);
        result = HTTPD_400_BAD_REQUEST;
        msg = "Certificate bundle error";
        goto cleanup;
    }
    certificate_bundle_iter_free(it);
    it = NULL;

    // secondary checks
    if (pk_count != 1)
    {
        ESP_LOGW(TAG, "Invalid number of keys in bundle : %i, exactly one is required", pk_count);
        result = HTTPD_400_BAD_REQUEST;
        msg = "Invalid number of keys";
        goto cleanup;
    }
    if (cert_count < 1)
    {
        ESP_LOGW(TAG, "Invalid number of certificates in bundle: %i, at least one is required", cert_count);
        result = HTTPD_400_BAD_REQUEST;
        msg = "Invalid number of certificates";
        goto cleanup;
    }

    // searching (and remove) certificates matching the private key
    for (int i = 0; i < cert_count; i++)
    {
        bool result = certificate_matches_private_key(certs[i].data, pks[0].data);
        ESP_LOGV(TAG, "certificate_matches_private_key %i", result);
        if (result)
        {
            leaf_cert = certs[i].data;
            certs[i].used = true;
            ESP_LOGI(TAG, "Found a certificate matching the private key");
        }
    }
    if (leaf_cert == NULL)
    {
        ESP_LOGW(TAG, "No certificate found matching provided private key");
        result = HTTPD_400_BAD_REQUEST;
        msg = "No certificate matching the private key";
        goto cleanup;
    }

    // search for parent certificate and build the certificate chain
    mbedtls_x509_crt *current_level = leaf_cert;
    for (int i = 0; i < cert_count; i++)
    {
        if (certs[i].used)
            continue;
        ESP_LOGV(TAG, "iter %i cert[i].data %p current_level %p", i, certs[i].data, current_level);

        // test parent against child
        uint32_t flags;
        int ret = mbedtls_x509_crt_verify(leaf_cert, certs[i].data, NULL, NULL, &flags, NULL, NULL);
        ESP_LOGD(TAG, "mbedtls_x509_crt_verify ret 0x%x flags 0x%08x", -ret, flags);
        if (ret != 0) // MBEDTLS_ERR_X509_CERT_VERIFY_FAILED
            continue;

        // candidate signed the current_level cert
        current_level->next = certs[i].data;
        current_level = certs[i].data;
        certs[i].used = true;

        // restart search
        i = 0;
    }

    // check for unused certs
    result = 0;
    for (int i = 0; i < cert_count; i++)
        if (!certs[i].used)
        {
            ESP_LOGW(TAG, "Found certificate not used in chain:\n%.*s", certs[i].pem_len, certs[i].pem_start);
            result++;
        }
    if (result != 0)
    {
        result = HTTPD_400_BAD_REQUEST;
        msg = "Unused certificates found";
        goto cleanup;
    }

    // TODO store in NVRAM

cleanup:
    // free leaf certificate and unused ones
    mbedtls_x509_crt_free(leaf_cert);
    for (int i = 0; i < cert_count; i++)
        if (!certs[i].used)
            mbedtls_x509_crt_free(certs[i].data);
    // free all private keys
    for (int i = 0; i < pk_count; i++)
        mbedtls_pk_free(pks[i].data);
    certificate_bundle_iter_free(it);
    free(buf);

    if (result != 0)
        return httpd_resp_send_err(req, result, msg);

    return httpd_resp_sendstr(req, "Certificate bundle successfully loaded");
}

esp_err_t serve_api_put_certificate_self_signed(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_put_certificate_self_signed version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    // restrict access
    if (!ofp_session_user_is_admin(req))
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Unauthorized");

    int ret = 0;
    unsigned char *output_buf = NULL;

    mbedtls_pk_context key;
    mbedtls_mpi N, P, Q, D, E, DP, DQ, QP;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;

    mbedtls_x509write_cert crt;
    mbedtls_mpi serial;
    char name[] = "CN=openfilpilote.local,O=local,C=FR";

    // generate private key (includes public too)

    mbedtls_mpi_init(&N);
    mbedtls_mpi_init(&P);
    mbedtls_mpi_init(&Q);
    mbedtls_mpi_init(&D);
    mbedtls_mpi_init(&E);
    mbedtls_mpi_init(&DP);
    mbedtls_mpi_init(&DQ);
    mbedtls_mpi_init(&QP);
    mbedtls_pk_init(&key);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    unsigned char custom[32];
    esp_fill_random(custom, sizeof(custom));
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, custom, sizeof(custom));
    if (ret != 0)
    {
        ESP_LOGW(TAG, "mbedtls_ctr_drbg_seed returned -0x%04x", (unsigned int)-ret);
        goto cleanup;
    }

    ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (ret != 0)
    {
        ESP_LOGW(TAG, "mbedtls_pk_setup returned -0x%04x", (unsigned int)-ret);
        goto cleanup;
    }

    ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(key), mbedtls_ctr_drbg_random, &ctr_drbg, SELF_SIGNED_CERT_RSA_KEY_SIZE, 65537);
    vTaskDelay(100 / portTICK_RATE_MS); // feed the watchdog ASAP
    if (ret != 0)
    {
        ESP_LOGW(TAG, "mbedtls_rsa_gen_key returned -0x%04x", (unsigned int)-ret);
        goto cleanup;
    }

    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(key);
    if (rsa == NULL)
    {
        ESP_LOGW(TAG, "mbedtls_pk_rsa returned NULL");
        goto cleanup;
    }

    ret = mbedtls_rsa_export(rsa, &N, &P, &Q, &D, &E);
    if (ret != 0)
    {
        ESP_LOGW(TAG, "could not export RSA parameters part1");
        goto cleanup;
    }

    ret = mbedtls_rsa_export_crt(rsa, &DP, &DQ, &QP);
    if (ret != 0)
    {
        ESP_LOGW(TAG, "could not export RSA parameters part 2");
        goto cleanup;
    }

    output_buf = malloc(SELF_SIGNED_CERT_OUTPUT_PEM_MAX_LENGTH);
    if (output_buf == NULL)
    {
        ESP_LOGW(TAG, "could not export RSA parameters part 2");
        goto cleanup;
    }

    memset(output_buf, 0, SELF_SIGNED_CERT_OUTPUT_PEM_MAX_LENGTH);
    ret = mbedtls_pk_write_key_pem(&key, output_buf, SELF_SIGNED_CERT_OUTPUT_PEM_MAX_LENGTH - 1);
    if (ret != 0)
    {
        ESP_LOGW(TAG, "writing pem key failed");
        goto cleanup;
    }

    ESP_LOGD(TAG, "PEM KEY\r\n%s", (char *)output_buf);
    kv_ns_set_str_atomic(kv_get_ns_ofp(), stor_key_https_key, (char *)output_buf);

    // generate self-signed certificate

    mbedtls_mpi_init(&serial);
    mbedtls_x509write_crt_init(&crt);

    ret = mbedtls_mpi_read_string(&serial, 10, SELF_SIGNED_CERT_SERIAL_NUMBER);
    if (ret != 0)
    {
        ESP_LOGW(TAG, "mbedtls_mpi_read_string returned -0x%04x", (unsigned int)-ret);
        goto cleanup;
    }

    mbedtls_x509write_crt_set_subject_key(&crt, &key);
    mbedtls_x509write_crt_set_issuer_key(&crt, &key);

    ret = mbedtls_x509write_crt_set_subject_name(&crt, name);
    if (ret != 0)
    {
        ESP_LOGW(TAG, "mbedtls_x509write_crt_set_subject_name returned -0x%04x", (unsigned int)-ret);
        goto cleanup;
    }

    ret = mbedtls_x509write_crt_set_issuer_name(&crt, name);
    if (ret != 0)
    {
        ESP_LOGW(TAG, "mbedtls_x509write_crt_set_issuer_name returned -0x%04x", (unsigned int)-ret);
        goto cleanup;
    }

    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);

    ret = mbedtls_x509write_crt_set_serial(&crt, &serial);
    if (ret != 0)
    {
        ESP_LOGW(TAG, "mbedtls_x509write_crt_set_serial returned -0x%04x", (unsigned int)-ret);
        goto cleanup;
    }

    ret = mbedtls_x509write_crt_set_validity(&crt, SELF_SIGNED_CERT_NOT_BEFORE, SELF_SIGNED_CERT_NOT_AFTER);
    if (ret != 0)
    {
        ESP_LOGW(TAG, "mbedtls_x509write_crt_set_validity returned -0x%04x", (unsigned int)-ret);
        goto cleanup;
    }

#if defined(MBEDTLS_SHA1_C)
    ret = mbedtls_x509write_crt_set_subject_key_identifier(&crt);
    if (ret != 0)
    {
        ESP_LOGW(TAG, "mbedtls_x509write_crt_set_subject_key_identifier returned -0x%04x", (unsigned int)-ret);
        goto cleanup;
    }

    ret = mbedtls_x509write_crt_set_authority_key_identifier(&crt);
    if (ret != 0)
    {
        ESP_LOGW(TAG, "mbedtls_x509write_crt_set_authority_key_identifier returned -0x%04x", (unsigned int)-ret);
        goto cleanup;
    }
#endif // MBEDTLS_SHA1_C

    ret = mbedtls_x509write_crt_pem(&crt, output_buf, SELF_SIGNED_CERT_OUTPUT_PEM_MAX_LENGTH - 1, mbedtls_ctr_drbg_random, &ctr_drbg);
    vTaskDelay(100 / portTICK_RATE_MS); // feed the watchdog
    if (ret < 0)
    {
        ESP_LOGW(TAG, "mbedtls_x509write_crt_pem returned -0x%04x", (unsigned int)-ret);
        goto cleanup;
    }

    ESP_LOGD(TAG, "PEM CERT\r\n%s", (char *)output_buf);
    kv_ns_set_str_atomic(kv_get_ns_ofp(), stor_key_https_certs, (char *)output_buf);

    ret = 0;

cleanup:
    free(output_buf);
    mbedtls_x509write_crt_free(&crt);
    mbedtls_mpi_free(&serial);

    mbedtls_mpi_free(&N);
    mbedtls_mpi_free(&P);
    mbedtls_mpi_free(&Q);
    mbedtls_mpi_free(&D);
    mbedtls_mpi_free(&E);
    mbedtls_mpi_free(&DP);
    mbedtls_mpi_free(&DQ);
    mbedtls_mpi_free(&QP);

    mbedtls_pk_free(&key);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);

    if (ret == 0)
        return httpd_resp_sendstr(req, "self-signed certificate generated successfully");
    else
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not generate self-signed certificate");
}
