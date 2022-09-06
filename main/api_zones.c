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
    cJSON *orders = cJSON_AddArrayToObject(root, json_key_orders);

    for (int i = 0; i < HW_OFP_ORDER_ID_ENUM_SIZE; i++)
    {
        const struct ofp_order_info *info = ofp_order_info_by_num_id(i);
        cJSON *order = cJSON_CreateObject();
        cJSON_AddStringToObject(order, stor_key_id, info->id);
        cJSON_AddStringToObject(order, stor_key_name, info->name);
        cJSON_AddStringToObject(order, stor_key_class, info->class);
        cJSON_AddItemToArray(orders, order);
    }

    httpd_resp_set_hdr(req, str_cache_control, str_private_max_age_600);

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

    struct ofp_hw *hw = ofp_hw_get_current();

    if (hw == NULL)
    {
        ESP_LOGD(TAG, "no zones");
        return httpd_resp_sendstr(req, "{ \"zones\": [] }");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *zones = cJSON_AddArrayToObject(root, "zones");
    for (int i = 0; i < hw->zone_set.count; i++)
    {
        struct ofp_zone *z = &hw->zone_set.zones[i];
        cJSON *zone = cJSON_CreateObject();
        cJSON_AddItemToArray(zones, zone);

        // id & desc
        cJSON_AddStringToObject(zone, json_key_id, z->id);
        cJSON_AddStringToObject(zone, json_key_description, z->description);

        // current
        const struct ofp_order_info *info = ofp_order_info_by_num_id(z->current);
        cJSON_AddStringToObject(zone, json_key_current, info->id);

        // mode
        char buf[24]; // ":fixed:cozyminus1\0" length is 20 but upgrade to avoid warning about id being 16
        switch (z->mode)
        {
        case HW_OFP_ZONE_MODE_FIXED:
            info = ofp_order_info_by_num_id(z->mode_data.order_id);
            snprintf(buf, sizeof(buf), ":fixed:%s", info->id);
            break;
        case HW_OFP_ZONE_MODE_PLANNING:
            snprintf(buf, sizeof(buf), ":planning:%i", z->mode_data.planning_id);
            break;
        default:
            break;
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Unknown zone mode");
        }
        cJSON_AddStringToObject(zone, json_key_mode, buf);
    }

    esp_err_t result = serve_json(req, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t serve_api_patch_zones_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    char *id = re_get_string(captures, 2);
    ESP_LOGD(TAG, "serve_api_patch_zones_id version=%i id=%s", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    // check zone
    struct ofp_hw *hw = ofp_hw_get_current();
    if (hw == NULL)
    {
        ESP_LOGD(TAG, "No hardware selected");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No hardware selected");
    }
    struct ofp_zone *zone = NULL, *candidate;
    for (int i = 0; i < hw->zone_set.count; i++)
    {
        candidate = &hw->zone_set.zones[i];
        if (strcmp(candidate->id, id) == 0)
        {
            ESP_LOGV(TAG, "zone found %i", i);
            zone = candidate;
            break;
        }
    }
    if (zone == NULL)
    {
        ESP_LOGD(TAG, "zone not found");
        return httpd_resp_send_404(req);
    }

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
            /*
             * BUG: there is something weird about the strlen of the error_ptr when parsing a empty string ()
             * V (12:08:14.016) webserver: Received 1 bytes
             * V (12:08:14.017) webserver: 0x3ffd570c   20 00 <-- incoming data
             * V (12:08:14.030) api_zones: --- parse error
             * V (12:08:14.031) api_zones: 0x3ffd570d   8a fb 3f dc <-- error_ptr
             * V (12:08:14.043) api_zones: 7 0x3ffd570d <-- strlen(error_ptr) error_ptr
             * D (12:08:14.044) api_zones: JSON parse error, before: ��?܊�? <-- logging with %s
             */
            ESP_LOGD(TAG, "JSON parse error, before: %s", error_ptr);
        }
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed parsing JSON body");
    }

    bool need_storing = false;

    // description
    cJSON *desc = cJSON_GetObjectItemCaseSensitive(root, json_key_description);
    if (desc != NULL)
    {
        if (!cJSON_IsString(desc) || (desc->valuestring == NULL))
        {
            ESP_LOGD(TAG, "Invalid type or value for element %s", json_key_description);
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameter");
        }

        ESP_LOGV(TAG, "description: %s", desc->valuestring);
        if (!ofp_zone_set_description(zone, desc->valuestring))
        {
            ESP_LOGW(TAG, "Could not set zone description");
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not set zone description");
        }

        need_storing = true;

        ESP_LOGV(TAG, "Description updated.");
    }

    // mode
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(root, json_key_mode);
    if (mode != NULL)
    {
        if (!cJSON_IsString(mode) || (mode->valuestring == NULL))
        {
            ESP_LOGD(TAG, "Invalid value for element %s", json_key_mode);
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameter");
        }

        ESP_LOGV(TAG, "mode: %s", mode->valuestring);
        struct re_result *res = re_match(parse_zone_mode_re_str, mode->valuestring);
        if (res == NULL || res->count != 6)
        {
            ESP_LOGD(TAG, "No match for mode in value %s for element %s", mode->valuestring, json_key_mode);
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameter");
        }

        char *fix = res->strings[2];
        char *fix_id = res->strings[3];
        char *plan = res->strings[4];
        char *plan_id = res->strings[5];

        if (fix != NULL && fix_id != NULL)
        {
            const struct ofp_order_info *info = ofp_order_info_by_str_id(fix_id);
            if (info == NULL)
            {
                ESP_LOGD(TAG, "Value %s for element %s is not an order id", mode->valuestring, json_key_mode);
                free(res);
                cJSON_Delete(root);
                return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameter");
            }

            if (!ofp_zone_set_mode_fixed(zone, info->order_id))
            {
                ESP_LOGW(TAG, "Could not set zone fixed mode");
                free(res);
                cJSON_Delete(root);
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not set zone fixed mode");
            }

            ESP_LOGV(TAG, "mode fix finished");
        }
        else if (plan != NULL && plan_id != NULL)
        {
            if (!ofp_zone_set_mode_planning(zone, atoi(plan_id)))
            {
                ESP_LOGW(TAG, "Could not set zone planning mode");
                free(res);
                cJSON_Delete(root);
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not set zone planning mode");
            }
            ESP_LOGV(TAG, "mode plan finished");
        }
        // no other valid case according to REGEX
        free(res);

        need_storing = true;
    }

    // not providing any matching element is not an error

    // cleanup
    cJSON_Delete(root);

    // update storage if needed
    if (need_storing && !ofp_zone_store(zone))
    {
        ESP_LOGW(TAG, "Could store updated zone");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could store updated zone");
    }

    return httpd_resp_sendstr(req, "");
}

esp_err_t serve_api_get_override(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_override version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    // get zone override from common namespace
    enum ofp_order_id order_id;
    bool active = ofp_override_get_order_id(&order_id);
    const struct ofp_order_info *info = NULL;
    if (active)
    {
        ESP_LOGV(TAG, "override is active");
        info = ofp_order_info_by_num_id(order_id);
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, stor_key_zone_override, info ? info->id : stor_val_none);

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
        ofp_override_disable();
        ofp_override_store();
        return httpd_resp_sendstr(req, "");
    }

    // generic values
    const struct ofp_order_info *info = ofp_order_info_by_str_id(override->valuestring);
    if (info == NULL)
    {
        ESP_LOGD(TAG, "Invalid order override %s", override->valuestring);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameter");
    }

    ESP_LOGD(TAG, "matched order %s", info->name);

    // set zone override in common namespace
    ofp_override_enable(info->order_id);
    ofp_override_store();

    // return success
    return httpd_resp_sendstr(req, "");
}