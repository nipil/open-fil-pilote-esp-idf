#include <cjson.h>
#include <esp_log.h>

#include "ofp.h"
#include "webserver.h"
#include "api_hw.h"
#include "storage.h"

static const char TAG[] = "api_hw";

const char stor_ns_hardware[] = "ofp_hardware";
const char stor_key_current[] = "current_id";

const char json_key_id[] = "id";
const char json_key_current[] = "current";
const char json_key_supported[] = "supported";
const char json_key_description[] = "description";

/***************************************************************************/

esp_err_t serve_api_get_hardware(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_hardware version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    // fetch current hardware id from storage, returns NULL if not found
    char *current_hw_id;
    kvh_get(current_hw_id, str, stor_ns_hardware, stor_key_current); // must be free'd after use

    // provide hardware list
    cJSON *root = cJSON_CreateObject();

    if (current_hw_id != NULL)
    {
        cJSON_AddStringToObject(root, json_key_current, current_hw_id);
        free(current_hw_id);
    }
    else
    {
        cJSON_AddNullToObject(root, json_key_current);
    }

    cJSON *supported = cJSON_AddArrayToObject(root, json_key_supported);
    for (int i = 0; i < ofp_hw_list_get_count(); i++)
    {
        struct ofp_hw *hw = ofp_hw_list_get_hw_by_index(i);
        cJSON *j = cJSON_CreateObject();
        cJSON_AddStringToObject(j, json_key_id, hw->id);
        cJSON_AddStringToObject(j, json_key_description, hw->description);
        cJSON_AddItemToArray(supported, j);
    }

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