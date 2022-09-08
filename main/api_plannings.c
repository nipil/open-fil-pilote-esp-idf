#include <cjson.h>
#include <esp_log.h>

#include "str.h"
#include "ofp.h"
#include "webserver.h"
#include "api_plannings.h"

static const char TAG[] = "api_plannings";

/***************************************************************************/

esp_err_t serve_api_get_plannings(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_plannings version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    struct ofp_planning_list *plan_list = ofp_planning_list_get();
    if (plan_list == NULL)
    {
        ESP_LOGW(TAG, "No planning list available");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Planning list not initialized");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *plannings = cJSON_AddArrayToObject(root, json_key_plannings);

    for (int i = 0; i < OFP_MAX_PLANNING_COUNT; i++)
    {
        struct ofp_planning *plan = plan_list->plannings[i];
        if (plan == NULL)
            continue;
        cJSON *planning = cJSON_CreateObject();
        cJSON_AddItemToArray(plannings, planning);
        cJSON_AddNumberToObject(planning, stor_key_id, plan->id);
        cJSON_AddStringToObject(planning, stor_key_name, plan->description);
    }

    esp_err_t result = serve_json(req, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t serve_api_post_plannings(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_post_plannings version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    struct ofp_planning_list *plan_list = ofp_planning_list_get();
    if (plan_list == NULL)
    {
        ESP_LOGW(TAG, "No planning list available");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Planning list not initialized");
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
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed parsing JSON body");

    // name is required
    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, stor_key_name);
    if (name == NULL)
    {
        ESP_LOGD(TAG, "Missing JSON element '%s'", stor_key_name);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing JSON element");
    }

    if (!cJSON_IsString(name) || (name->valuestring == NULL))
    {
        ESP_LOGD(TAG, "Invalid type or value for element %s", stor_key_name);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameter");
    }

    ESP_LOGV(TAG, "name: %s", name->valuestring);
    if (!ofp_planning_list_add_new_planning(name->valuestring))
    {
        ESP_LOGW(TAG, "Could not create new planning '%s'", name->valuestring);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not create new planning");
    }
    ESP_LOGV(TAG, "Planning created.");

    // not providing any matching element is not an error

    cJSON_Delete(root);
    return httpd_resp_sendstr(req, "");
}

esp_err_t serve_api_get_plannings_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    ESP_LOGD(TAG, "serve_api_get_plannings_id version=%i id=%i", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    struct ofp_planning_list *plan_list = ofp_planning_list_get();
    if (plan_list == NULL)
    {
        ESP_LOGW(TAG, "No planning list available");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Planning list not initialized");
    }

    int planning_id = atoi(captures->strings[2]);
    ESP_LOGD(TAG, "planning_id %i", planning_id);
    struct ofp_planning *plan = ofp_planning_list_find_planning_by_id(planning_id);
    if (plan == NULL)
    {
        ESP_LOGD(TAG, "planning not found");
        return httpd_resp_send_404(req);
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *slots = cJSON_AddArrayToObject(root, json_key_slots);
    for (int i = 0; i < OFP_MAX_PLANNING_SLOT_COUNT; i++)
    {
        struct ofp_planning_slot *slot = plan->slots[i];
        if (slot == NULL)
            continue;

        const struct ofp_order_info *info = ofp_order_info_by_num_id(slot->order_id);
        if (info == NULL)
        {
            ESP_LOGW(TAG, "Invalid order id %i found in slot %ih%i, skipping it", slot->order_id, slot->hour, slot->minute);
            continue;
        }

        cJSON *s = cJSON_CreateObject();
        cJSON_AddItemToArray(slots, s);
        cJSON_AddNumberToObject(s, json_key_id, slot->id);
        cJSON_AddNumberToObject(s, json_key_dow, slot->dow);
        cJSON_AddNumberToObject(s, json_key_hour, slot->hour);
        cJSON_AddNumberToObject(s, json_key_minute, slot->minute);
        cJSON_AddStringToObject(s, json_key_order, info->id);
    }

    esp_err_t result = serve_json(req, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t serve_api_patch_plannings_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    ESP_LOGD(TAG, "serve_api_patch_plannings_id version=%i id=%i", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    struct ofp_planning *plan = ofp_planning_list_find_planning_by_id(id);
    if (plan == NULL)
        return httpd_resp_send_404(req);

    // read
    char *buf = webserver_get_request_data_atomic(req);
    if (buf == NULL)
    {
        ESP_LOGW(TAG, "Failed getting request data");
        return ESP_FAIL;
    }
    ESP_LOGV(TAG, "Request data: %s", buf);

    // parse
    cJSON *root = cJSON_Parse(buf);

    // cleanup
    free(buf);

    // parse error
    if (root == NULL)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed parsing JSON body");

    // optional name
    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, stor_key_name);
    if (name != NULL)
    {
        if (!cJSON_IsString(name) || name->valuestring == NULL)
        {
            ESP_LOGD(TAG, "Invalid type or value for element %s", stor_key_name);
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameter");
        }
        if (!ofp_planning_change_description(plan->id, name->valuestring))
        {
            ESP_LOGW(TAG, "Could not set description for planning %i: %s", plan->id, name->valuestring);
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not set description for planning");
        }
    }

    // not providing any matching element is not an error

    cJSON_Delete(root);
    return httpd_resp_sendstr(req, "");
}

esp_err_t serve_api_delete_plannings_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    ESP_LOGD(TAG, "serve_api_delete_plannings_id version=%i id=%i", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    if (ofp_planning_list_find_planning_by_id(id) == NULL)
        return httpd_resp_send_404(req);

    if (!ofp_planning_list_remove_planning(id))
        return httpd_resp_send_500(req);

    return httpd_resp_sendstr(req, "");
}

esp_err_t serve_api_post_plannings_id_slots(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    ESP_LOGD(TAG, "serve_api_post_plannings_id_slots version=%i id=%i", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    // read
    char *buf = webserver_get_request_data_atomic(req);
    if (buf == NULL)
    {
        ESP_LOGW(TAG, "Failed getting request data");
        return ESP_FAIL;
    }
    ESP_LOGV(TAG, "Request data: %s", buf);

    // parse
    cJSON *root = cJSON_Parse(buf);

    // cleanup
    free(buf);

    // dow is required
    cJSON *j_dow = cJSON_GetObjectItemCaseSensitive(root, json_key_dow);
    if (j_dow == NULL)
    {
        ESP_LOGD(TAG, "Missing JSON element '%s'", json_key_dow);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing JSON element");
    }
    if (!cJSON_IsNumber(j_dow) || !ofp_day_of_week_is_valid(j_dow->valueint))
    {
        ESP_LOGD(TAG, "Invalid type or value for element %s", json_key_dow);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameter");
    }
    enum ofp_day_of_week dow = j_dow->valueint;
    ESP_LOGV(TAG, "dow: %i", dow);

    // hour is required
    cJSON *j_hour = cJSON_GetObjectItemCaseSensitive(root, json_key_hour);
    if (j_hour == NULL)
    {
        ESP_LOGD(TAG, "Missing JSON element '%s'", json_key_hour);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing JSON element");
    }
    if (!cJSON_IsNumber(j_hour) || j_hour->valueint < 0 || j_hour->valueint >= 24)
    {
        ESP_LOGD(TAG, "Invalid type or value for element %s", json_key_hour);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameter");
    }
    int hour = j_hour->valueint;
    ESP_LOGV(TAG, "hour: %i", hour);

    // minute is required
    cJSON *j_minute = cJSON_GetObjectItemCaseSensitive(root, json_key_minute);
    if (j_minute == NULL)
    {
        ESP_LOGD(TAG, "Missing JSON element '%s'", json_key_minute);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing JSON element");
    }
    if (!cJSON_IsNumber(j_minute) || j_minute->valueint < 0 || j_minute->valueint >= 24)
    {
        ESP_LOGD(TAG, "Invalid type or value for element %s", json_key_minute);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameter");
    }
    int minute = j_minute->valueint;
    ESP_LOGV(TAG, "minute: %i", minute);

    // required mode
    enum ofp_zone_mode found_mode = HW_OFP_ZONE_MODE_ENUM_SIZE;
    union ofp_zone_mode_data found_mode_data = {.planning_id = -1};
    cJSON *j_mode = cJSON_GetObjectItemCaseSensitive(root, json_key_mode);
    if (j_mode == NULL)
    {
        ESP_LOGD(TAG, "Missing JSON element '%s'", json_key_minute);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing JSON element");
    }
    if (!cJSON_IsString(j_mode) || (j_mode->valuestring == NULL))
    {
        ESP_LOGD(TAG, "Invalid value for element %s", json_key_mode);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameter");
    }

    ESP_LOGV(TAG, "mode: %s", j_mode->valuestring);
    struct re_result *res = re_match(parse_zone_mode_re_str, j_mode->valuestring);
    if (res == NULL || res->count != 6)
    {
        ESP_LOGD(TAG, "No match for mode in value %s for element %s", j_mode->valuestring, json_key_mode);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid mode string");
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
            ESP_LOGD(TAG, "Value %s is not an order id", fix_id);
            free(res);
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid fixed mode");
        }

        found_mode = HW_OFP_ZONE_MODE_FIXED;
        found_mode_data.order_id = info->order_id;
        ESP_LOGV(TAG, "mode fixe %i", found_mode_data.order_id);
    }
    else if (plan != NULL && plan_id != NULL)
    {
        int planning_id = atoi(plan_id);

        if (ofp_planning_list_find_planning_by_id(planning_id) == NULL)
        {
            ESP_LOGD(TAG, "Invalid planning id %i ", planning_id);
            free(res);
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid planning id");
        }

        found_mode = HW_OFP_ZONE_MODE_PLANNING;
        found_mode_data.planning_id = planning_id;
        ESP_LOGV(TAG, "mode plan %i", found_mode_data.planning_id);
    }

    // no other valid case according to REGEX
    free(res);

    // every data has been extracted from JSON
    cJSON_Delete(root);

    // slots only allow fixed orderb
    if (found_mode != HW_OFP_ZONE_MODE_FIXED)
    {
        ESP_LOGD(TAG, "Invalid mode %i", found_mode);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid mode");
    }

    // add
    if (!ofp_planning_add_new_slot(id, dow, hour, minute, found_mode_data.order_id))
    {
        ESP_LOGW(TAG, "Could not add new slot to planning %i", id);
        return false;
    }

    return httpd_resp_sendstr(req, "");
}

esp_err_t serve_api_patch_plannings_id_slots_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    int slot_id = re_get_int(captures, 3);
    ESP_LOGD(TAG, "serve_api_patch_plannings_id_slots_id version=%i id=%i slot_id=%i", version, id, slot_id);
    if (version != 1)
        return httpd_resp_send_404(req);

    // TODO: not yet implemented

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_delete_plannings_id_slots_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    int slot_id = re_get_int(captures, 3);
    ESP_LOGD(TAG, "serve_api_delete_plannings_id_slots_id version=%i id=%i slot_id=%i", version, id, slot_id);
    if (version != 1)
        return httpd_resp_send_404(req);

    // TODO: not yet implemented

    return httpd_resp_send_500(req);
}