#include <cjson.h>

#include "ofp.h"
#include "webserver.h"
#include "api_hw.h"

/***************************************************************************/

esp_err_t serve_api_get_hardware(httpd_req_t *req, struct re_result *captures)
{
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