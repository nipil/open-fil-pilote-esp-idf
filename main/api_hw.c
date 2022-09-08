#include <cjson.h>
#include <esp_log.h>

#include "sdkconfig.h"

#include "str.h"
#include "ofp.h"
#include "webserver.h"
#include "api_hw.h"
#include "storage.h"

static const char TAG[] = "api_hw";

/***************************************************************************/

struct ofp_hw *ofp_get_hardware_from_stored_id(void)
{
    // get hardware from common namespace
    char *current_hw_id = kv_ns_get_str_atomic(kv_get_ns_ofp(), stor_key_hardware_type); // must be free'd after use
    if (current_hw_id == NULL)
    {
        free(current_hw_id);
        return NULL;
    }

    struct ofp_hw *hw = ofp_hw_list_find_hw_by_id(current_hw_id);
    free(current_hw_id);

    return hw;
}

/***************************************************************************/

esp_err_t serve_api_get_hardware(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_hardware version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    // fetch current hardware id from storage, returns NULL if not found
    char *current_hw_id = kv_ns_get_str_atomic(kv_get_ns_ofp(), stor_key_hardware_type); // must be free'd after use

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

    httpd_resp_set_hdr(req, str_cache_control, str_private_max_age_600);

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

    // build hardware target namespace
    char tmp_hw_ns_name[NVS_NS_NAME_MAX_SIZE];
    if (!kv_build_ns_hardware(hw->id, tmp_hw_ns_name))
    {
        ESP_LOGE(TAG, "Could not build hardware namespace");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Hardware name too long");
    }

    // provide list of parameters
    char *str;
    int num;
    nvs_handle_t h = kv_open_ns(tmp_hw_ns_name);
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
        case HW_OFP_PARAM_TYPE_INTEGER:
            num = kv_get_i32(h, param->id, param->value.int_);
            cJSON_AddStringToObject(j, json_key_type, json_type_number);
            cJSON_AddNumberToObject(j, json_key_value, num);
            break;
        case HW_OFP_PARAM_TYPE_STRING:
            str = kv_get_str(h, param->id);
            if (str == NULL)
                str = param->value.string_;
            cJSON_AddStringToObject(j, json_key_type, json_type_string);
            cJSON_AddStringToObject(j, json_key_value, str);
            break;
        default:
            ESP_LOGW(TAG, "Invalid ofp_hw_param_type value detected: %i for parameter %s of hardware %s", param->type, param->id, hw->id);
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid hardware parameter value detected");
        }
    }
    kv_close(h);

    httpd_resp_set_hdr(req, str_cache_control, str_private_max_age_600);

    esp_err_t result = serve_json(req, root);
    cJSON_Delete(root);
    return result;
}

/***************************************************************************/

/*
 * stores hardware parameters (NEEDS REBOOT TO TAKE INTO ACCOUNT)
 * 'hardware_type' param name is reserved, and sets hardware type for next reboot
 * other parameter names, if match hardware, will be updated in NVS for next reboot
 *
 * Every parameter present in the hardware definition is required to be present
 * in the request or the request will be considered invalir. No partial updates.
 */
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
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Malformed request body");
    }

    // get the requested hardware
    char *form_hw_current = form_data_get_str(data, stor_key_hardware_type);
    if (form_hw_current == NULL)
    {
        ESP_LOGD(TAG, "Missing parameter '%s'", stor_key_hardware_type); // TODO: give incorrect value in msg
        form_data_free(data);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Hardware type not provided");
    }

    // search if the requested hardware is known
    struct ofp_hw *hw = ofp_hw_list_find_hw_by_id(form_hw_current);
    if (hw == NULL)
    {
        ESP_LOGD(TAG, "Unknown hardware '%s'", form_hw_current);
        form_data_free(data);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unknown hardware type"); // TODO: give incorrect value in msg
    }

    // iterate desired hardware parameters
    for (int i = 0; i < hw->param_count; i++)
    {
        const char *hw_param_id = hw->params[i].id;
        ESP_LOGV(TAG, "search for form parameter %s", hw_param_id);
        char *form_param_value = form_data_get_str(data, hw_param_id);

        // search for hardware parameter
        if (form_param_value == NULL)
        {
            ESP_LOGD(TAG, "Missing parameter '%s'", hw_param_id); // TODO: give incorrect value in msg
            form_data_free(data);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing parameter");
        }

        // verify parameter type if needed
        int value;
        if (hw->params[i].type == HW_OFP_PARAM_TYPE_INTEGER)
        {
            if (!parse_int(form_param_value, &value))
            {
                ESP_LOGD(TAG, "Invalid format for integer parameter '%s': %s", hw_param_id, form_param_value);
                form_data_free(data);
                return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameter");
            }
        }
    }

    // if we reached here, everything is correct, store the updated parameters without checking for errors
    ESP_LOGV(TAG, "Request is valid, storing data");

    // store new hardware type in common namespace
    nvs_handle_t h = kv_open_ns(kv_get_ns_ofp());
    kv_set_str(h, stor_key_hardware_type, form_hw_current);
    kv_commit(h);
    kv_close(h);

    // build hardware target namespace
    char tmp_hw_ns_name[NVS_NS_NAME_MAX_SIZE];
    if (!kv_build_ns_hardware(form_hw_current, tmp_hw_ns_name))
    {
        ESP_LOGE(TAG, "Could not build hardware namespace");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Hardware name too long");
    }

    h = kv_open_ns(tmp_hw_ns_name);
    // clear namespace before saving
    kv_clear(h);
    // store hardware parameters in dedicated hardware namespace
    for (int i = 0; i < hw->param_count; i++)
    {
        struct ofp_hw_param *hw_param = &hw->params[i];
        char *form_param_value = form_data_get_str(data, hw_param->id);
        int n;
        switch (hw_param->type)
        {
        case HW_OFP_PARAM_TYPE_INTEGER:
            parse_int(form_param_value, &n);
            kv_set_i32(h, hw_param->id, n);
            break;
        case HW_OFP_PARAM_TYPE_STRING:
            kv_set_str(h, hw_param->id, form_param_value);
            break;
        default:
            ESP_LOGW(TAG, "Invalid ofp_hw_param_type value detected: %i. Skip storing parameter %s for hardware %s", hw_param->type, hw_param->id, form_hw_current);
            break;
        }
    }
    kv_commit(h);
    kv_close(h);

    // cleanup
    form_data_free(data);

    // then we will reboot
    return serve_redirect(req, "/ofp-api/v1/reboot");
}