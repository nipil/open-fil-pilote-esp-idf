#ifndef API_ACCOUNTS_H
#define API_ACCOUNTS_H

#include <esp_https_server.h>

#include "utils.h"

esp_err_t serve_api_get_accounts(httpd_req_t *req, struct re_result *captures);

esp_err_t serve_api_post_accounts(httpd_req_t *req, struct re_result *captures);

esp_err_t serve_api_delete_accounts_id(httpd_req_t *req, struct re_result *captures);

esp_err_t serve_api_patch_accounts_id(httpd_req_t *req, struct re_result *captures);

#endif /* API_ACCOUNTS_H */