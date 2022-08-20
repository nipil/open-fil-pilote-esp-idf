#ifndef API_HW_H
#define API_HW_H

#include <esp_https_server.h>

#include "utils.h"

esp_err_t serve_api_get_hardware(httpd_req_t *req, struct re_result *captures);

#endif /* API_HW_H */