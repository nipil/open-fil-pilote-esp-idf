#include <cjson.h>
#include <esp_log.h>

#include "str.h"
#include "ofp.h"
#include "webserver.h"
#include "api_accounts.h"
#include "storage.h"

static const char TAG[] = "api_zones";

/***************************************************************************/

esp_err_t serve_api_get_orders(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_orders version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    cJSON *root = cJSON_CreateObject();
    cJSON *orders = cJSON_AddArrayToObject(root, "orders");

    for (int i = 0; i < HW_OFP_ORDER_ID_ENUM_SIZE; i++)
    {
        const struct ofp_order_info *info = ofp_order_info_by_num_id(i);
        cJSON *order = cJSON_CreateObject();
        cJSON_AddStringToObject(order, stor_key_id, info->id);
        cJSON_AddStringToObject(order, stor_key_name, info->name);
        cJSON_AddStringToObject(order, stor_key_class, info->class);
        cJSON_AddItemToArray(orders, order);
    }

    esp_err_t result = serve_json(req, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t serve_api_get_zones(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_zones version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_patch_zones_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    char *id = re_get_string(captures, 2);
    ESP_LOGD(TAG, "serve_api_patch_zones_id version=%i id=%s", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_get_override(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_override version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    // get zone override from common namespace
    char *override = kv_ns_get_str_atomic(kv_get_ns_ofp(), stor_key_zone_override); // must be free'd after use

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, stor_key_zone_override, override ? override : stor_val_none);
    if (override)
        free(override);

    // TODO: manage cache ?

    esp_err_t result = serve_json(req, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t serve_api_put_override(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_put_override version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    // read
    char *buf = webserver_get_request_data_atomic(req);
    if (buf == NULL)
    {
        ESP_LOGW(TAG, "Failed getting request data");
        return ESP_FAIL;
    }

    // parse
    cJSON *root = cJSON_Parse(buf);

    // cleanup
    free(buf);

    // parse error
    if (root == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            ESP_LOGD(TAG, "JSON parse error, before: %s", error_ptr);
        }
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed parsing JSON body");
    }

    // required string parameter
    cJSON *override = cJSON_GetObjectItemCaseSensitive(root, stor_key_zone_override);
    if (override == NULL)
    {
        ESP_LOGD(TAG, "Missing JSON element %s", stor_key_zone_override);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed parsing JSON body");
    }
    if (!cJSON_IsString(override) || (override->valuestring == NULL))
    {
        ESP_LOGD(TAG, "Invalid type or value for element %s", stor_key_zone_override);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameter");
    }

    // special value
    if (strcmp(override->valuestring, stor_val_none) == 0)
    {
        // set zone override in common namespace
        kv_ns_set_str_atomic(kv_get_ns_ofp(), stor_key_zone_override, override->valuestring);
        return httpd_resp_sendstr(req, "");
    }

    // generic values
    const struct ofp_order_info *info = ofp_order_info_by_str_id(override->valuestring);
    if (override == NULL)
    {
        ESP_LOGD(TAG, "Invalid order override %s", override->valuestring);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameter");
    }

    ESP_LOGD(TAG, "matched order %s", info->name);
    // set zone override in common namespace
    kv_ns_set_str_atomic(kv_get_ns_ofp(), stor_key_zone_override, override->valuestring);

    // return success
    return httpd_resp_sendstr(req, "");
}