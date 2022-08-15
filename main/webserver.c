#include <stdio.h>
#include <esp_log.h>
#include <esp_https_server.h>

#include "webserver.h"

/* @brief TAG_MAIN used for ESP serial console messages */
const char TAG_WEB[] = "webserver";

/* constants for efficient memory management */
const static char http_content_type_html[] = "text/html";
const static char http_content_type_js[] = "text/javascript";
const static char route_root[] = "/";
const static char route_ofp_html[] = "/ofp.html";
const static char route_ofp_js[] = "/ofp.js";

/* const httpd related values stored in ROM */
const static char http_302_hdr[] = "302 Found";
const static char http_location_hdr[] = "Location";

/* HTTPS server handle */
httpd_handle_t *app_server = NULL;

/***************************************************************************/

esp_err_t serve_from_asm(httpd_req_t *req, const unsigned char *binary_start, const unsigned char *binary_end, const char *http_content_type)
{
    size_t binary_len = binary_end - binary_start;

    ESP_LOGI(TAG_WEB, "start %p end %p size %i", binary_start, binary_end, binary_len);
    httpd_resp_set_type(req, http_content_type);
    httpd_resp_send(req, (const char *)binary_start, binary_len);
    return ESP_OK;
}

/***************************************************************************/

esp_err_t https_handler_get(httpd_req_t *req)
{
    if (strcmp(req->uri, route_root) == 0)
    {
        httpd_resp_set_type(req, http_content_type_html);
        httpd_resp_set_status(req, http_302_hdr);
        httpd_resp_set_hdr(req, http_location_hdr, route_ofp_html);
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    if (strcmp(req->uri, route_ofp_html) == 0)
    {
        // This does NOT need to be null-terminated
        extern const unsigned char ofp_html_start[] asm("_binary_ofp_html_start");
        extern const unsigned char ofp_html_end[] asm("_binary_ofp_html_end");
        return serve_from_asm(req, ofp_html_start, ofp_html_end, http_content_type_html);
    }

    if (strcmp(req->uri, route_ofp_js) == 0)
    {
        // This does NOT need to be null-terminated
        extern const unsigned char ofp_js_start[] asm("_binary_ofp_js_start");
        extern const unsigned char ofp_js_end[] asm("_binary_ofp_js_end");
        return serve_from_asm(req, ofp_js_start, ofp_js_end, http_content_type_js);
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "{ }", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t https_handler_post(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "<h1>post</h1>", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t https_handler_put(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "<h1>put</h1>", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t https_handler_patch(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "<h1>patch</h1>", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t https_handler_delete(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "<h1>delete</h1>", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/***************************************************************************/

void webserver_register_wildcard_method(httpd_handle_t *new_server, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r))
{
    // we can use a local variable as httpd_register_uri_handler copies the content of httpd_uri_t
    const httpd_uri_t request = {
        .uri = "*",
        .method = method,
        .handler = handler};
    ESP_ERROR_CHECK(httpd_register_uri_handler(new_server, &request));
}

void webserver_register_uri_handlers(httpd_handle_t new_server)
{
    // use only generic handlers to avoid consuming too many handlers
    webserver_register_wildcard_method(new_server, HTTP_GET, https_handler_get);
    webserver_register_wildcard_method(new_server, HTTP_POST, https_handler_post);
    webserver_register_wildcard_method(new_server, HTTP_PUT, https_handler_put);
    webserver_register_wildcard_method(new_server, HTTP_PATCH, https_handler_patch);
    webserver_register_wildcard_method(new_server, HTTP_DELETE, https_handler_delete);
}

/***************************************************************************/

void webserver_start()
{
    httpd_handle_t new_server = NULL;

    if (app_server)
    {
        ESP_LOGW(TAG_WEB, "Webserver already started, restarting.");
        webserver_stop();
    }

    // Start the httpd server
    ESP_LOGI(TAG_WEB, "Starting server");

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();

    // This need to be null-terminated
    extern const unsigned char cacert_pem_start[] asm("_binary_cacert_pem_start");
    extern const unsigned char cacert_pem_end[] asm("_binary_cacert_pem_end");
    conf.cacert_pem = cacert_pem_start;
    conf.cacert_len = cacert_pem_end - cacert_pem_start;

    // This need to be null-terminated
    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[] asm("_binary_prvtkey_pem_end");
    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    // dedicated webserver control port, for easier coexistance with other servers
    conf.httpd.ctrl_port = CONFIG_OFP_UI_WEBSERVER_CONTROL_PORT;

    // use only wildcard matcher to reduce number of handlers
    conf.httpd.uri_match_fn = httpd_uri_match_wildcard;

    // errors happening here are due to faulty design
    ESP_ERROR_CHECK(httpd_ssl_start(&new_server, &conf));

    // register generic handles
    webserver_register_uri_handlers(new_server);

    // persist handle
    app_server = new_server;
}

void webserver_stop()
{
    if (!app_server)
        return;

    ESP_LOGI(TAG_WEB, "Stopping webserver.");
    httpd_ssl_stop(app_server);
    app_server = NULL;
}