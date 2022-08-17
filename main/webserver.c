#include <stdio.h>
#include <esp_log.h>
#include <esp_https_server.h>
#include <cjson.h>

#include "httpd_basic_auth.h"

#include "webserver.h"
#include "ofp.h"

static const char TAG[] = "webserver";

/* constants for efficient memory management */
static const char http_content_type_html[] = "text/html";
static const char http_content_type_js[] = "text/javascript";
static const char http_content_type_json[] = "application/json";
static const char route_root[] = "/";
static const char route_ofp_html[] = "/ofp.html";
static const char route_ofp_js[] = "/ofp.js";
static const char route_api_hw[] = "/ofp-api/v1/hardware"; // TODO: avoid repeating base API path

static const char http_302_hdr[] = "302 Found";
static const char http_location_hdr[] = "Location";

/* HTTPS server handle */
static httpd_handle_t *app_server = NULL;

/***************************************************************************/

static esp_err_t serve_from_asm(httpd_req_t *req, const unsigned char *binary_start, const unsigned char *binary_end, const char *http_content_type)
{
    /*
     * Static pages are TXT files which are likely to be embedded using EMBED_TXTFILES,
     * which adds a final NULL terminator, and increase the size by one.
     * This is likely done so that the embedded file could safely be printed.
     *
     * When serving it through the network, do *NOT* send this final NULL terminator
     */
    size_t binary_len = binary_end - binary_start - 1;

    ESP_LOGD(TAG, "Serve_from_asm start %p end %p size %i", binary_start, binary_end, binary_len);
    httpd_resp_set_type(req, http_content_type);
    httpd_resp_send(req, (const char *)binary_start, binary_len);
    return ESP_OK;
}

static esp_err_t serve_static_ofp_html(httpd_req_t *req)
{
    extern const unsigned char ofp_html_start[] asm("_binary_ofp_html_start");
    extern const unsigned char ofp_html_end[] asm("_binary_ofp_html_end");
    return serve_from_asm(req, ofp_html_start, ofp_html_end, http_content_type_html);
}

static esp_err_t serve_static_ofp_js(httpd_req_t *req)
{
    extern const unsigned char ofp_js_start[] asm("_binary_ofp_js_start");
    extern const unsigned char ofp_js_end[] asm("_binary_ofp_js_end");
    return serve_from_asm(req, ofp_js_start, ofp_js_end, http_content_type_js);
}

/***************************************************************************/

static esp_err_t serve_redirect(httpd_req_t *req, char *target)
{
    httpd_resp_set_status(req, http_302_hdr);
    httpd_resp_set_hdr(req, http_location_hdr, target);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t serve_json(httpd_req_t *req, cJSON *node)
{
    const char *txt = cJSON_Print(node);
    ESP_LOGD(TAG, "Serving serialized JSON: %s", txt);
    httpd_resp_set_type(req, http_content_type_json);
    esp_err_t result = httpd_resp_sendstr(req, txt);
    free((void *)txt); // we are responsible for freeing the rendering buffer
    return result;
}

/***************************************************************************/

static esp_err_t serve_api_hardware(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();

    cJSON *supported = cJSON_CreateArray();
    for (int i = 0; i < ofp_hw_list_get_count(); i++)
    {
        struct ofp_hw *hw = ofp_hw_list_get_hw_by_index(i);
        cJSON *j = cJSON_CreateObject();
        cJSON_AddStringToObject(j, "id", hw->id);
        cJSON_AddStringToObject(j, "description", hw->description);
        cJSON_AddItemToArray(supported, j);
    }
    cJSON_AddItemToObject(root, "supported", supported);

    cJSON *current = cJSON_CreateNull(); // TODO: replace with current value
    cJSON_AddItemToObject(root, "current", current);

    // TODO: manage cache ?

    esp_err_t result = serve_json(req, root);
    cJSON_Delete(root);
    return result;
}

/***************************************************************************/

static esp_err_t https_handler_get(httpd_req_t *req)
{
    // redirect root to static content
    if (strcmp(req->uri, route_root) == 0)
        return serve_redirect(req, (char *)route_ofp_html);

    // static content
    if (strcmp(req->uri, route_ofp_html) == 0)
        return serve_static_ofp_html(req);

    if (strcmp(req->uri, route_ofp_js) == 0)
        return serve_static_ofp_js(req);

    // httpd_resp_set_type(req, http_content_type_html);

    // api content
    if (strcmp(req->uri, route_api_hw) == 0)
        return serve_api_hardware(req);


    return httpd_resp_send_404(req);
}

static esp_err_t https_handler_post(httpd_req_t *req)
{
    return httpd_resp_send_404(req);
}

static esp_err_t https_handler_put(httpd_req_t *req)
{
    return httpd_resp_send_404(req);
}

static esp_err_t https_handler_patch(httpd_req_t *req)
{
    return httpd_resp_send_404(req);
}

static esp_err_t https_handler_delete(httpd_req_t *req)
{
    return httpd_resp_send_404(req);
}

/***************************************************************************/

#ifdef CONFIG_OFP_UI_WEBSERVER_REQUIRES_AUTHENTICATION

static bool is_authentication_valid(httpd_req_t *req)
{
    return httpd_basic_auth(req, "admin", "admin") == ESP_OK;
}

static esp_err_t authentication_reject(httpd_req_t *req)
{
    httpd_basic_auth_resp_send_401(req);
    httpd_resp_sendstr(req, "Not Authorized");
    return ESP_FAIL;
}

#endif

/***************************************************************************/

static esp_err_t https_handler_generic(httpd_req_t *req)
{

#ifdef CONFIG_OFP_UI_WEBSERVER_REQUIRES_AUTHENTICATION
    // check credentials
    if (!is_authentication_valid(req))
        return authentication_reject(req);
#endif

    // handle requests
    if (req->method == HTTP_GET)
        return https_handler_get(req);
    if (req->method == HTTP_GET)
        return https_handler_post(req);
    if (req->method == HTTP_POST)
        return https_handler_put(req);
    if (req->method == HTTP_PATCH)
        return https_handler_patch(req);
    if (req->method == HTTP_DELETE)
        return https_handler_delete(req);

    // default
    return httpd_resp_send_404(req);
}

/***************************************************************************/

static void webserver_register_wildcard_method(httpd_handle_t *new_server, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r))
{
    // we can use a local variable as httpd_register_uri_handler copies the content of httpd_uri_t
    const httpd_uri_t request = {
        .uri = "*",
        .method = method,
        .handler = handler};
    ESP_ERROR_CHECK(httpd_register_uri_handler(new_server, &request));
}

static void webserver_register_uri_handlers(httpd_handle_t new_server)
{
    // use only generic handlers to avoid consuming too many handlers
    webserver_register_wildcard_method(new_server, HTTP_GET, https_handler_generic);
    webserver_register_wildcard_method(new_server, HTTP_POST, https_handler_generic);
    webserver_register_wildcard_method(new_server, HTTP_PUT, https_handler_generic);
    webserver_register_wildcard_method(new_server, HTTP_PATCH, https_handler_generic);
    webserver_register_wildcard_method(new_server, HTTP_DELETE, https_handler_generic);
}

/***************************************************************************/

void webserver_start(void)
{
    httpd_handle_t new_server = NULL;

    if (app_server)
    {
        ESP_LOGW(TAG, "Webserver already started, restarting.");
        webserver_stop();
    }

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server");

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

void webserver_stop(void)
{
    if (!app_server)
        return;

    ESP_LOGI(TAG, "Stopping webserver.");
    httpd_ssl_stop(app_server);
    app_server = NULL;
}