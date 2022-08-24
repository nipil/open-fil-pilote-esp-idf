#include <cjson.h>
#include <esp_log.h>

#include "sdkconfig.h"

#include "ofp.h"
#include "webserver.h"
#include "api_hw.h"
#include "storage.h"

static const char TAG[] = "api_hw";

static const char stor_ns_hardware[] = "ofp_hardware";
static const char stor_key_current[] = "current_id";

static const char json_key_current[] = "current";
static const char json_key_description[] = "description";
static const char json_key_id[] = "id";
static const char json_key_parameters[] = "parameters";
static const char json_key_supported[] = "supported";
static const char json_key_type[] = "type";
static const char json_key_value[] = "value";

static const char json_type_number[] = "number";
static const char json_type_string[] = "string";

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
        cJSON_AddItemToArray(supported, j);
        cJSON_AddStringToObject(j, json_key_id, hw->id);
        cJSON_AddStringToObject(j, json_key_description, hw->description);
    }

    // TODO: manage cache ?

    esp_err_t result = serve_json(req, root);
    cJSON_Delete(root);
    return result;
}

/***************************************************************************/

esp_err_t serve_api_get_hardware_id_parameters(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    char *id = re_get_string(captures, 2);
    ESP_LOGD(TAG, "serve_api_get_hardware_id_parameters version=%i id=%s", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    // find hardware
    struct ofp_hw *hw = ofp_hw_list_find_hw_by_id(id);

    // nothing found
    if (hw == NULL)
        return httpd_resp_send_404(req);

    assert(hw->params != NULL);

    // provide list of parameters
    cJSON *root = cJSON_CreateObject();
    cJSON *parameters = cJSON_AddArrayToObject(root, json_key_parameters);
    for (int i = 0; i < hw->param_count; i++)
    {
        struct ofp_hw_param *param = &hw->params[i];
        cJSON *j = cJSON_CreateObject();
        cJSON_AddItemToArray(parameters, j);
        cJSON_AddStringToObject(j, json_key_id, param->id);
        cJSON_AddStringToObject(j, json_key_description, param->description);
        switch (param->type)
        {
        case HW_OFP_PARAM_INTEGER:
            cJSON_AddStringToObject(j, json_key_type, json_type_number);
            cJSON_AddNumberToObject(j, json_key_value, param->value.int_);
            break;
        case HW_OFP_PARAM_STRING:
            cJSON_AddStringToObject(j, json_key_type, json_type_string);
            cJSON_AddStringToObject(j, json_key_value, param->value.string_);
            break;
        default:
            cJSON_Delete(root);
            return httpd_resp_send_500(req);
        }
    }

    // TODO: manage cache ?

    esp_err_t result = serve_json(req, root);
    cJSON_Delete(root);
    return result;
}

/***************************************************************************/

esp_err_t serve_api_post_hardware(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "Serve_api_post_hardware version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    // get params
    struct ofp_form_data *data = webserver_form_data_from_req(req);
    if (data == NULL)
    {
        ESP_LOGD(TAG, "Error parsing x-www-form-urlencoded data");
        return httpd_resp_send_err(req, 400, "Malformed request body");
    }

    // dump
    ESP_LOGD(TAG, "Param count %d", data->count);
    for (int i = 0; i < data->count; i++)
    {
        struct ofp_form_param *param = &data->params[i];
        ESP_LOGD(TAG, "name '%s' value '%s'", param->name ? param->name : "NULL", param->value ? param->value : "NULL");
    }

    // TODO: match param name with expected
    // TODO: check value type and convert if needed

    // cleanup
    form_data_free(data);

    return httpd_resp_sendstr(req, "Success");
}