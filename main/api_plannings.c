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

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_post_plannings(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_post_plannings version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_get_plannings_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    ESP_LOGD(TAG, "serve_api_get_plannings_id version=%i id=%i", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_patch_plannings_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    ESP_LOGD(TAG, "serve_api_patch_plannings_id version=%i id=%i", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_delete_plannings_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    ESP_LOGD(TAG, "serve_api_delete_plannings_id version=%i id=%i", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_post_plannings_id_slots(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    int id = re_get_int(captures, 2);
    ESP_LOGD(TAG, "serve_api_post_plannings_id_slots version=%i id=%i", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

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

    return httpd_resp_send_500(req);
}