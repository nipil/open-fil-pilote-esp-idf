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

    // TODO: not yet implemented

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_delete_plannings_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    ESP_LOGD(TAG, "serve_api_delete_plannings_id version=%i id=%i", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    // TODO: not yet implemented

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_post_plannings_id_slots(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    ESP_LOGD(TAG, "serve_api_post_plannings_id_slots version=%i id=%i", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    // TODO: not yet implemented

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_put_plannings_id_slots_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    char *slot_id = re_get_string(captures, 3);
    ESP_LOGD(TAG, "serve_api_put_plannings_id_slots_id version=%i id=%i slot_id=%s", version, id, slot_id);
    if (version != 1)
        return httpd_resp_send_404(req);

    // TODO: not yet implemented

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_delete_plannings_id_slots_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    char *slot_id = re_get_string(captures, 3);
    ESP_LOGD(TAG, "serve_api_delete_plannings_id_slots_id version=%i id=%i slot_id=%s", version, id, slot_id);
    if (version != 1)
        return httpd_resp_send_404(req);

    // TODO: not yet implemented

    return httpd_resp_send_500(req);
}