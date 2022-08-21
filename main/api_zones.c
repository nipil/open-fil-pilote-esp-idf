#include <cjson.h>
#include <esp_log.h>

#include "ofp.h"
#include "webserver.h"
#include "api_accounts.h"

static const char TAG[] = "api_zones";

/***************************************************************************/

esp_err_t serve_api_get_orders(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_orders version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    return httpd_resp_send_500(req);
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

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_put_override(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_put_override version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    return httpd_resp_send_500(req);
}
