#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <cjson.h>
#include <esp_https_server.h>

esp_err_t serve_redirect(httpd_req_t *req, char *target);
esp_err_t serve_json(httpd_req_t *req, cJSON *node);

void webserver_start(void);
void webserver_stop(void);

/* set up flag preventing web server from serving any new request */
void webserver_disable(void);

/* received the required amount of data from incoming request body */
esp_err_t webserver_read_request_data(httpd_req_t *req, char *buf, size_t len);

/*
 * THIS FUNCTION SHOULD BE USED ONLY EXCEPTIONNALY FOR LARGE BUFFERS
 * Prefer the function below, as most use-cases do not need large buffers
 * and memory allocation could fail... so do NOT use it for everyday tasks
 *
 * Received the required amount of data from incoming request body
 * Dynamically alloc a buff and ADD NULL terminator
 *
 * Memory MUST BE FREED BY CALLER !
 */
char *webserver_get_request_data_atomic_max_size(httpd_req_t *req, const int max_size);

/*
 * This function is the "go-to" one to be used
 * Same as above, but with implicit
 * Maximum size = CONFIG_OFP_UI_WEBSERVER_DATA_MAX_SIZE_SINGLE_OP
 *
 * Memory MUST BE FREED BY CALLER !
 */
char *webserver_get_request_data_atomic(httpd_req_t *req);

/*
 * receive and parse form data from incoming request
 * MUST BE FREED by the caller using form_data_free() from utils.h
 */
struct ofp_form_data *webserver_form_data_from_req(httpd_req_t *req);

/* this is a static page waiting for device to be up (polls status) */
esp_err_t serve_static_ofp_wait_html(httpd_req_t *req);

/* session context */
bool ofp_session_user_is_admin(httpd_req_t *req);
bool ofp_session_user_is_admin_or_self(httpd_req_t *req, const char *user_id);
char *ofp_session_get_user(httpd_req_t *req);
char *ofp_session_get_source_ip_address(httpd_req_t *req);

/* header helper */
char *ofp_webserver_get_header_string(httpd_req_t *req, const char *field);

#endif /* WEBSERVER_H */