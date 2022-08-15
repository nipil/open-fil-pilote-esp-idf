#include <stdio.h>
#include <esp_log.h>
#include <esp_https_server.h>

/* authorisation HTTP 401 */
#include "httpd_basic_auth.h"

#include "webserver.h"

/* @brief TAG_MAIN used for ESP serial console messages */
const char TAG_WEB[] = "webserver";

/* HTTPS server handle */
httpd_handle_t *app_server = NULL;

/***************************************************************************/

/* An HTTP GET handler */
esp_err_t https_handler_private(httpd_req_t *req)
{

    if (!httpd_basic_auth(req, "https", "https") == ESP_OK)
    {
        httpd_basic_auth_resp_send_401(req);
        httpd_resp_sendstr(req, "Not Authorized");
        return ESP_FAIL;
    }

    extern const unsigned char ofp_html_start[] asm("_binary_ofp_html_start");
    extern const unsigned char ofp_html_end[] asm("_binary_ofp_html_end");
    size_t ofp_html_len = ofp_html_end - ofp_html_start;

    ESP_LOGI(TAG_WEB, "%i bytes in HTML", ofp_html_len);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)ofp_html_start, ofp_html_len);

    return ESP_OK;
}

/* HTTPS root handle */
esp_err_t https_handler_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "<h1>root</h1>", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
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

    extern const unsigned char cacert_pem_start[] asm("_binary_cacert_pem_start");
    extern const unsigned char cacert_pem_end[] asm("_binary_cacert_pem_end");
    conf.cacert_pem = cacert_pem_start;
    conf.cacert_len = cacert_pem_end - cacert_pem_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[] asm("_binary_prvtkey_pem_end");
    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    // dedicated webserver control port, for easier coexistance with other servers
    conf.httpd.ctrl_port = CONFIG_OFP_UI_WEBSERVER_CONTROL_PORT;

    // E (9142) httpd: httpd_server_init: error in creating ctrl socket (112)
    ESP_ERROR_CHECK(httpd_ssl_start(&new_server, &conf));

    // Set URI handlers
    ESP_LOGI(TAG_WEB, "Registering URI handlers");

    // Register root URI handler
    const httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = https_handler_root};

    ESP_ERROR_CHECK(httpd_register_uri_handler(new_server, &root));

    const httpd_uri_t private = {
        .uri = "/private",
        .method = HTTP_GET,
        .handler = https_handler_private};

    ESP_ERROR_CHECK(httpd_register_uri_handler(new_server, &private));

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