#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <cjson.h>
#include <esp_https_server.h>

esp_err_t serve_redirect(httpd_req_t *req, char *target);
esp_err_t serve_json(httpd_req_t *req, cJSON *node);

void webserver_start(void);
void webserver_stop(void);

/* received the required amount of data from incoming request body */
esp_err_t get_request_data(httpd_req_t *req, char *buf, size_t len);

#endif /* WEBSERVER_H */