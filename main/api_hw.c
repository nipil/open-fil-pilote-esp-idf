#include <cjson.h>
#include <esp_log.h>

#include "ofp.h"
#include "webserver.h"
#include "api_hw.h"

static const char TAG[] = "api_hw";

/***************************************************************************/

esp_err_t serve_api_get_hardware(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_hardware version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    cJSON *root = cJSON_CreateObject();

    cJSON *supported = cJSON_CreateArray();
    for (int i = 0; i < ofp_hw_list_get_count(); i++)
    {
        struct ofp_hw *hw = ofp_hw_list_get_hw_by_index(i);
        cJSON *j = cJSON_CreateObject();
        cJSON_AddStringToObject(j, "id", hw->id);
        cJSON_AddStringToObject(j, "description", hw->description);
        cJSON_AddItemToArray(supported, j);
    }
    cJSON_AddItemToObject(root, "supported", supported);

    cJSON *current = cJSON_CreateNull(); // TODO: replace with current value
    cJSON_AddItemToObject(root, "current", current);

    // TODO: manage cache ?

    esp_err_t result = serve_json(req, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t serve_api_get_hardware_id_parameters(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    char *id = re_get_string(captures, 2);
    ESP_LOGD(TAG, "serve_api_get_hardware_id_parameters version=%i id=%s", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_post_hardware(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_post_hardware version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    return httpd_resp_send_500(req);
}