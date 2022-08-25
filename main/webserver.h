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
esp_err_t webserver_get_request_data(httpd_req_t *req, char *buf, size_t len);

/*
 * receive and parse form data from incoming request
 * MUST BE FREED by the caller using form_data_free() from utils.h
 */
struct ofp_form_data *webserver_form_data_from_req(httpd_req_t *req);

/* this is a static page waiting for device to be up (polls status) */
esp_err_t serve_static_ofp_wait_html(httpd_req_t *req);

#endif /* WEBSERVER_H */