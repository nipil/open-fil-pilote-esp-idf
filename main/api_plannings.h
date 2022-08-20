#ifndef API_PLANNINGS_H
#define API_PLANNINGS_H

#include <esp_https_server.h>

#include "utils.h"

esp_err_t serve_api_get_plannings(httpd_req_t *req, struct re_result *captures);
esp_err_t serve_api_post_plannings(httpd_req_t *req, struct re_result *captures);

esp_err_t serve_api_get_plannings_id(httpd_req_t *req, struct re_result *captures);
esp_err_t serve_api_patch_plannings_id(httpd_req_t *req, struct re_result *captures);
esp_err_t serve_api_delete_plannings_id(httpd_req_t *req, struct re_result *captures);

esp_err_t serve_api_post_plannings_id_slots(httpd_req_t *req, struct re_result *captures);

esp_err_t serve_api_put_plannings_id_slots_id(httpd_req_t *req, struct re_result *captures);
esp_err_t serve_api_delete_plannings_id_slots_id(httpd_req_t *req, struct re_result *captures);

#endif /* API_PLANNINGS_H */