#include <stdio.h>
#include <esp_log.h>
#include <esp_https_server.h>

#include "httpd_basic_auth.h"

#include "webserver.h"
#include "ofp.h"
#include "utils.h"
#include "api_hw.h"
#include "api_accounts.h"
#include "api_zones.h"
#include "api_mgmt.h"
#include "api_plannings.h"

static const char TAG[] = "webserver";

/* constants for efficient memory management */

static const char http_content_type_html[] = "text/html";
static const char http_content_type_js[] = "text/javascript";
static const char http_content_type_json[] = "application/json";

static const char route_root[] = "/";
static const char route_ofp_html[] = "/ofp.html";
static const char route_ofp_js[] = "/ofp.js";

static const char route_api_hardware[] = "^/ofp-api/v([[:digit:]]+)/hardware$";
static const char route_api_hardware_id_parameters[] = "^/ofp-api/v([[:digit:]]+)/hardware/([[:alnum:]]+)/parameters$";

static const char route_api_accounts[] = "^/ofp-api/v([[:digit:]]+)/accounts$";
static const char route_api_accounts_id[] = "^/ofp-api/v([[:digit:]]+)/accounts/([[:alnum:]]+)$";

static const char route_api_orders[] = "^/ofp-api/v([[:digit:]]+)/orders$";
static const char route_api_override[] = "^/ofp-api/v([[:digit:]]+)/override$";
static const char route_api_zones[] = "^/ofp-api/v([[:digit:]]+)/zones$";
static const char route_api_zones_id[] = "^/ofp-api/v([[:digit:]]+)/zones/([[:alnum:]]+)$";

static const char route_api_upgrade[] = "^/ofp-api/v([[:digit:]]+)/upgrade$";
static const char route_api_status[] = "^/ofp-api/v([[:digit:]]+)/status$";

static const char route_api_plannings[] = "^/ofp-api/v([[:digit:]]+)/plannings$";
static const char route_api_planning_id[] = "^/ofp-api/v([[:digit:]]+)/plannings/([[:digit:]]+)$";
static const char route_api_planning_id_slots[] = "^/ofp-api/v([[:digit:]]+)/plannings/([[:digit:]]+)/slots$";
static const char route_api_planning_id_slots_id[] = "^/ofp-api/v([[:digit:]]+)/plannings/([[:digit:]]+)/slots/((2[0-3]|[0-1][[:digit:]])h[0-5][[:digit:]])$";

static const char http_302_hdr[] = "302 Found";

static const char http_location_hdr[] = "Location";

/* HTTPS server handle */
static httpd_handle_t *app_server = NULL;

/***************************************************************************/

/* template for every API handler */
typedef esp_err_t (*api_serve_func)(httpd_req_t *req, struct re_result *captures);

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

esp_err_t serve_redirect(httpd_req_t *req, char *target)
{
    httpd_resp_set_status(req, http_302_hdr);
    httpd_resp_set_hdr(req, http_location_hdr, target);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t serve_json(httpd_req_t *req, cJSON *node)
{
    const char *txt = cJSON_Print(node);
    ESP_LOGD(TAG, "Serving serialized JSON: %s", txt);

    httpd_resp_set_type(req, http_content_type_json);
    esp_err_t result = httpd_resp_sendstr(req, txt);

    free((void *)txt); // we are responsible for freeing the rendering buffer
    return result;
}

/***************************************************************************/

/*
 * Helper function for API entrypoints
 *
 * Tries to match the provided regex to the given URL
 * If it matches, calls the api handler function
 * 
 * Returns true if the regex matches (ie. no other route should be tested), false otherwise
 * Updates result with the handler value, so that caller can act upon it or forward to caller
 */
static bool api_route_try(esp_err_t *result, httpd_req_t *req, const char *re_str, api_serve_func handler)
{
    struct re_result *captures = re_match(re_str, req->uri);
    if (captures != NULL)
    {
        *result = handler(req, captures);
        re_free(captures);
        return true;
    }
    return false;
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

    // api content
    esp_err_t result;

    if (api_route_try(&result, req, route_api_hardware, serve_api_get_hardware))
        return result;

    if (api_route_try(&result, req, route_api_hardware_id_parameters, serve_api_get_hardware_id_parameters))
        return result;

    if (api_route_try(&result, req, route_api_accounts, serve_api_get_accounts))
        return result;

    if (api_route_try(&result, req, route_api_orders, serve_api_get_orders))
        return result;

    if (api_route_try(&result, req, route_api_zones, serve_api_get_zones))
        return result;

    if (api_route_try(&result, req, route_api_override, serve_api_get_override))
        return result;

    if (api_route_try(&result, req, route_api_status, serve_api_get_status))
        return result;

    if (api_route_try(&result, req, route_api_plannings, serve_api_get_plannings))
        return result;

    if (api_route_try(&result, req, route_api_planning_id, serve_api_get_plannings_id))
        return result;

    return httpd_resp_send_404(req);
}

static esp_err_t https_handler_post(httpd_req_t *req)
{
    esp_err_t result;

    if (api_route_try(&result, req, route_api_hardware, serve_api_post_hardware))
        return result;

    if (api_route_try(&result, req, route_api_accounts, serve_api_post_accounts))
        return result;

    if (api_route_try(&result, req, route_api_upgrade, serve_api_post_upgrade))
        return result;

    if (api_route_try(&result, req, route_api_plannings, serve_api_post_plannings))
        return result;

    if (api_route_try(&result, req, route_api_planning_id_slots, serve_api_post_plannings_id_slots))
        return result;

    return httpd_resp_send_404(req);
}

static esp_err_t https_handler_put(httpd_req_t *req)
{
    esp_err_t result;

    if (api_route_try(&result, req, route_api_override, serve_api_put_override))
        return result;

    if (api_route_try(&result, req, route_api_planning_id_slots_id, serve_api_put_plannings_id_slots_id))
        return result;

    return httpd_resp_send_404(req);
}

static esp_err_t https_handler_patch(httpd_req_t *req)
{
    esp_err_t result;

    if (api_route_try(&result, req, route_api_accounts_id, serve_api_patch_accounts_id))
        return result;

    if (api_route_try(&result, req, route_api_zones_id, serve_api_patch_zones_id))
        return result;

    if (api_route_try(&result, req, route_api_planning_id, serve_api_patch_plannings_id))
        return result;

    return httpd_resp_send_404(req);
}

static esp_err_t https_handler_delete(httpd_req_t *req)
{
    esp_err_t result;

    if (api_route_try(&result, req, route_api_accounts_id, serve_api_delete_accounts_id))
        return result;

    if (api_route_try(&result, req, route_api_planning_id, serve_api_delete_plannings_id))
        return result;

    if (api_route_try(&result, req, route_api_planning_id_slots_id, serve_api_delete_plannings_id_slots_id))
        return result;

    return httpd_resp_send_404(req);
}

/***************************************************************************/

#ifdef CONFIG_OFP_UI_WEBSERVER_REQUIRES_AUTHENTICATION

static bool is_authentication_valid(httpd_req_t *req)
{
    // TODO: check actual accounts
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
    if (req->method == HTTP_POST)
        return https_handler_post(req);
    if (req->method == HTTP_PUT)
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