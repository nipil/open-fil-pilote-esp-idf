#ifndef API_ZONES_H
#define API_ZONES_H

#include <esp_https_server.h>

#include "utils.h"

esp_err_t serve_api_get_orders(httpd_req_t *req, struct re_result *captures);

esp_err_t serve_api_get_zones(httpd_req_t *req, struct re_result *captures);

esp_err_t serve_api_patch_zones_id(httpd_req_t *req, struct re_result *captures);

esp_err_t serve_api_get_override(httpd_req_t *req, struct re_result *captures);

esp_err_t serve_api_put_override(httpd_req_t *req, struct re_result *captures);

#endif /* API_ZONES_H */