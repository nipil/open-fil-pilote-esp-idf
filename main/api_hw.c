#include <cjson.h>
#include <esp_log.h>

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
    ESP_LOGD(TAG, "serve_api_post_hardware version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    ESP_LOGD(TAG, "content length %i", req->content_len);

    int remaining = req->content_len;
    const int buff_size = 64;
    char buf[buff_size];

    // fetch post data
    do
    {
        // one block at a time
        int amount = min_int(buff_size, remaining);
        ESP_LOGD(TAG, "trying to fetching %i bytes", amount);
        int ret = httpd_req_recv(req, buf, amount);

        // check for errors
        if (ret <= 0)
        {
            /* 0 return value indicates connection closed */
            if (ret == 0)
            {
                ESP_LOGE(TAG, "httpd_req_recv error: connection closed");
                return httpd_resp_send_500(req);
            }

            /* Check if timeout occurred */
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                /* In case of timeout one can choose to retry calling
                 * httpd_req_recv(), but to keep it simple, here we
                 * respond with an HTTP 408 (Request Timeout) error */
                ESP_LOGE(TAG, "httpd_req_recv error: connection timeout");
                httpd_resp_send_408(req);
            }

            /* In case of error, returning ESP_FAIL will
             * ensure that the underlying socket is closed */
            ESP_LOGE(TAG, "httpd_req_recv error: %s", esp_err_to_name(ret));
            return httpd_resp_send_500(req);
        }

        ESP_LOGD(TAG, "received %i bytes", ret);

        // display buffer
        ESP_LOG_BUFFER_HEXDUMP(TAG, buf, ret, ESP_LOG_DEBUG);
        remaining -= ret;
        ESP_LOGD(TAG, "Remaining %i bytes", remaining);

    } while (remaining > 0);

    // Content-Type: application/x-www-form-urlencoded
    // foo azérty ! toto % caca @ choubinou " di#ze
    // hardware_type=ESP32&hardware_parameter_name_sample_param=foo+az%C3%A9rty+%21+toto+%25+caca+%40+choubinou+%22+di%23ze&hardware_parameter_name_another_param=42

    return httpd_resp_sendstr(req, "Success");
}