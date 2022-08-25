#include <cjson.h>
#include <esp_log.h>

#include "ofp.h"
#include "webserver.h"
#include "api_accounts.h"
#include "storage.h"

static const char TAG[] = "api_zones";

static const char stor_ns_ofp[] = "ofp";

static const char stor_key_zone_override[] = "override";
static const char stor_key_id[] = "id";
static const char stor_key_name[] = "name";
static const char stor_key_class[] = "class";

/***************************************************************************/

esp_err_t serve_api_get_orders(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_orders version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    cJSON *root = cJSON_CreateObject();
    cJSON *orders = cJSON_AddArrayToObject(root, "orders");

    for (int i = 0; i < HW_OFP_ORDER_ID_ENUM_SIZE; i++)
    {
        const struct ofp_order_info *info = ofp_order_info_by_num_id(i);
        cJSON *order = cJSON_CreateObject();
        cJSON_AddStringToObject(order, stor_key_id, info->id);
        cJSON_AddStringToObject(order, stor_key_name, info->name);
        cJSON_AddStringToObject(order, stor_key_class, info->class);
        cJSON_AddItemToArray(orders, order);
    }

    esp_err_t result = serve_json(req, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t serve_api_get_zones(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_zones version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_patch_zones_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    char *id = re_get_string(captures, 2);
    ESP_LOGD(TAG, "serve_api_patch_zones_id version=%i id=%s", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_get_override(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_override version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    char *override;
    kvh_get(override, str, stor_ns_ofp, stor_key_zone_override); // must be free'd after use

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, stor_key_zone_override, override ? override : "none");
    if (override)
        free(override);

    // TODO: manage cache ?

    esp_err_t result = serve_json(req, root);
    cJSON_Delete(root);
    return result;
}

esp_err_t serve_api_put_override(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_put_override version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    return httpd_resp_send_500(req);
}
