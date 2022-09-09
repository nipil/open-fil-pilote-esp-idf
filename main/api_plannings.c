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
    char *name = NULL;
    if (cjson_get_child_string(root, stor_key_name, &name) != JSON_HELPER_RESULT_SUCCESS)
    {
        ESP_LOGD(TAG, "Invalid name");
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid name");
    }

    ESP_LOGV(TAG, "name: %s", name);
    if (!ofp_planning_list_add_new_planning(name))
    {
        ESP_LOGW(TAG, "Could not create new planning '%s'", name);
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

    ESP_LOGD(TAG, "planning_id %i", id);
    struct ofp_planning *plan = ofp_planning_list_find_planning_by_id(id);
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

    // name is optional
    char *name = NULL;
    if (cjson_get_child_string(root, stor_key_name, &name) == JSON_HELPER_RESULT_INVALID)
    {
        ESP_LOGD(TAG, "Invalid name");
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid name");
    }

    if (name != NULL && !ofp_planning_change_description(plan->id, name))
    {
        ESP_LOGW(TAG, "Could not set description for planning %i: %s", plan->id, name);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not set description for planning");
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
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not remove planning");

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

    int itmp;

    // dow is required
    if (cjson_get_child_int(root, json_key_dow, &itmp) != JSON_HELPER_RESULT_SUCCESS || !ofp_day_of_week_is_valid(itmp))
    {
        ESP_LOGD(TAG, "Invalid dow");
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid dow");
    }
    enum ofp_day_of_week dow = itmp;
    ESP_LOGV(TAG, "dow: %i", dow);

    // hour is required
    if (cjson_get_child_int(root, json_key_hour, &itmp) != JSON_HELPER_RESULT_SUCCESS || itmp < 0 || itmp >= 24)
    {
        ESP_LOGD(TAG, "Invalid hour");
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid hour");
    }
    int hour = itmp;
    ESP_LOGV(TAG, "hour: %i", hour);

    // minute is required
    if (cjson_get_child_int(root, json_key_minute, &itmp) != JSON_HELPER_RESULT_SUCCESS || itmp < 0 || itmp >= 60)
    {
        ESP_LOGD(TAG, "Invalid minute");
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid minute");
    }
    int minute = itmp;
    ESP_LOGV(TAG, "minute: %i", minute);

    // order is required
    char *order = NULL;
    if (cjson_get_child_string(root, json_key_order, &order) != JSON_HELPER_RESULT_SUCCESS || order == NULL)
    {
        ESP_LOGD(TAG, "Invalid order");
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid order");
    }

    const struct ofp_order_info *info = ofp_order_info_by_str_id(order);
    if (info == NULL)
    {
        ESP_LOGD(TAG, "Invalid order %s", order);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid order");
    }

    ESP_LOGV(TAG, "dow %i hour %i minute %i order_id %i", dow, hour, minute, info->order_id);

    // every data has been extracted from JSON
    cJSON_Delete(root);

    // add
    if (!ofp_planning_add_new_slot(id, dow, hour, minute, info->order_id))
    {
        ESP_LOGW(TAG, "Could not add new slot to planning %i", id);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not add new slot to planning");
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

    int itmp;

    // dow is optional
    enum ofp_day_of_week dow = OFP_DOW_ENUM_SIZE;
    enum json_helper_result jres = cjson_get_child_int(root, json_key_dow, &itmp);
    if (jres != JSON_HELPER_RESULT_NOT_FOUND)
    {
        if (jres == JSON_HELPER_RESULT_INVALID || !ofp_day_of_week_is_valid(itmp))
        {
            ESP_LOGD(TAG, "Invalid dow");
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid dow");
        }
        dow = itmp;
        ESP_LOGV(TAG, "dow: %i", dow);
        if (!ofp_planning_slot_set_dow(id, slot_id, dow))
        {
            ESP_LOGD(TAG, "Could not set planning dow %i", dow);
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not set slot dow");
        }
    }

    // hour is optional
    int hour = -1;
    jres = cjson_get_child_int(root, json_key_hour, &itmp);
    if (jres != JSON_HELPER_RESULT_NOT_FOUND)
    {
        if (jres == JSON_HELPER_RESULT_INVALID || itmp < 0 || itmp >= 60)
        {
            ESP_LOGD(TAG, "Invalid hour");
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid hour");
        }
        hour = itmp;
        ESP_LOGV(TAG, "hour: %i", hour);
        if (!ofp_planning_slot_set_hour(id, slot_id, hour))
        {
            ESP_LOGD(TAG, "Could not set planning hour %i", hour);
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not set slot hour");
        }
    }

    // minute is optional
    int minute = -1;
    jres = cjson_get_child_int(root, json_key_minute, &itmp);
    if (jres != JSON_HELPER_RESULT_NOT_FOUND)
    {
        if (jres == JSON_HELPER_RESULT_INVALID || itmp < 0 || itmp >= 60)
        {
            ESP_LOGD(TAG, "Invalid minute");
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid minute");
        }
        minute = itmp;
        ESP_LOGV(TAG, "minute: %i", minute);
        if (!ofp_planning_slot_set_minute(id, slot_id, minute))
        {
            ESP_LOGD(TAG, "Could not set planning minute %i", minute);
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not set slot minute");
        }
    }

    // order is optional
    char *order = NULL;
    if (cjson_get_child_string(root, json_key_order, &order) == JSON_HELPER_RESULT_SUCCESS)
    {
        const struct ofp_order_info *info = ofp_order_info_by_str_id(order);
        if (info == NULL)
        {
            ESP_LOGD(TAG, "Invalid order");
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid order");
        }
        ESP_LOGV(TAG, "order: %i", info->order_id);
        if (!ofp_planning_slot_set_order(id, slot_id, info->order_id))
        {
            ESP_LOGD(TAG, "Could not set slot order %i", info->order_id);
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not set slot order");
        }
    }

    // not providing any matching element is not an error

    cJSON_Delete(root);

    return httpd_resp_sendstr(req, "");
}

esp_err_t serve_api_delete_plannings_id_slots_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    int slot_id = re_get_int(captures, 3);
    ESP_LOGD(TAG, "serve_api_delete_plannings_id_slots_id version=%i id=%i slot_id=%i", version, id, slot_id);
    if (version != 1)
        return httpd_resp_send_404(req);

    if (!ofp_planning_remove_existing_slot(id, slot_id))
    {
        ESP_LOGW(TAG, "Could not remove slot %i from planning %i", slot_id, id);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not remove slot");
    }

    return httpd_resp_sendstr(req, "");
}