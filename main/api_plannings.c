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

    // No list available, means provide
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

    // TODO: not yet implemented

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_get_plannings_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    ESP_LOGD(TAG, "serve_api_get_plannings_id version=%i id=%i", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    // TODO: not yet implemented

    return httpd_resp_send_500(req);
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