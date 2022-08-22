#ifndef API_MGMT_H
#define API_MGMT_H

#include <esp_https_server.h>

#include "utils.h"

esp_err_t serve_api_get_status(httpd_req_t *req, struct re_result *captures);

esp_err_t serve_api_post_upgrade(httpd_req_t *req, struct re_result *captures);

#endif /* API_MGMT_H */