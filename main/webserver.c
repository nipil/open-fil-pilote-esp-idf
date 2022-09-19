#include <stdio.h>
#include <esp_log.h>
#include <esp_https_server.h>
#include <mbedtls/base64.h>

#include "str.h"
#include "webserver.h"
#include "ofp.h"
#include "utils.h"
#include "api_hw.h"
#include "api_accounts.h"
#include "api_zones.h"
#include "api_mgmt.h"
#include "api_plannings.h"

static const char TAG[] = "webserver";

/* HTTPS server handle */
static httpd_handle_t *app_server = NULL;

/* flag to enable/disable httpd serving globally */
static bool serving_enabled = true;

/***************************************************************************/

/* template for every API handler */
typedef esp_err_t (*api_serve_func)(httpd_req_t *req, struct re_result *captures);

/***************************************************************************/

/* disable web serving */
void webserver_disable(void)
{
    portMUX_TYPE mutex_serving_enabled = portMUX_INITIALIZER_UNLOCKED;
    taskENTER_CRITICAL(&mutex_serving_enabled);
    serving_enabled = false;
    taskEXIT_CRITICAL(&mutex_serving_enabled);
}

/* disable web serving */
static bool webserver_is_enabled(void)
{
    bool ret;
    portMUX_TYPE mutex_serving_enabled = portMUX_INITIALIZER_UNLOCKED;
    taskENTER_CRITICAL(&mutex_serving_enabled);
    ret = serving_enabled;
    taskEXIT_CRITICAL(&mutex_serving_enabled);
    return ret;
}

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

    ESP_LOGV(TAG, "Serve_from_asm start %p end %p size %i", binary_start, binary_end, binary_len);
    httpd_resp_set_hdr(req, str_cache_control, str_private_max_age_600);
    // httpd_resp_set_hdr(req, str_cache_control, str_private_no_store);
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

esp_err_t serve_static_ofp_wait_html(httpd_req_t *req)
{
    extern const unsigned char ofp_wait_html_start[] asm("_binary_ofp_wait_html_start");
    extern const unsigned char ofp_wait_html_end[] asm("_binary_ofp_wait_html_end");
    return serve_from_asm(req, ofp_wait_html_start, ofp_wait_html_end, http_content_type_html);
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

    /*
     * This one is a GET because we can not redirect to POST
     * and by choice because any tool can do a GET
     */
    if (api_route_try(&result, req, route_api_reboot, serve_api_get_reboot))
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

    if (api_route_try(&result, req, route_api_planning_id_slots_id, serve_api_patch_plannings_id_slots_id))
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

static bool is_authentication_valid(httpd_req_t *req)
{
    ESP_LOGV(TAG, "is_authentication_valid");

    bool result = false;

    // cleanup variables
    char *auth_head = NULL;
    struct re_result *res = NULL;
    uint8_t *b64d = NULL;

    size_t auth_head_len = 1 + httpd_req_get_hdr_value_len(req, http_authorization_hdr);
    if (auth_head_len <= 1 + 7)
        goto cleanup;

    auth_head = malloc(auth_head_len);
    if (auth_head == NULL)
        goto cleanup;

    esp_err_t ret = httpd_req_get_hdr_value_str(req, http_authorization_hdr, auth_head, auth_head_len);
    if (ret != ESP_OK)
        goto cleanup;

    ESP_LOGV(TAG, "Header: %s", auth_head);

    res = re_match(parse_authorization_401_re_str, auth_head);
    if (res == NULL)
        goto cleanup;

    free(auth_head);
    auth_head = NULL;

    char *b64e = re_get_string(res, 1);
    size_t olen, ilen = strlen(b64e);
    int r = mbedtls_base64_decode(NULL, 0, &olen, (uint8_t *)b64e, ilen);
    ESP_LOGV(TAG, "r %i olen %i", r, olen);
    if (r != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL)
        goto cleanup;

    b64d = malloc(olen + 1);
    if (b64d == NULL)
        goto cleanup;

    r = mbedtls_base64_decode(b64d, olen + 1, &olen, (uint8_t *)b64e, ilen);
    if (r != 0)
        goto cleanup;

    re_free(res);
    res = NULL;
    b64e = NULL;

    b64d[olen] = '\0';
    ESP_LOGV(TAG, "Decoded: %s", b64d);

    res = re_match(parse_credentials_401_re_str, (char *)b64d);
    if (res == NULL)
        goto cleanup;

    free(b64d);
    b64d = NULL;

    char *username = re_get_string(res, 1);
    char *cleartext = re_get_string(res, 2);
    ESP_LOGV(TAG, "username %s password %s", username, cleartext);

    struct ofp_account *account = ofp_account_list_find_account_by_id(username);
    ESP_LOGV(TAG, "account %p", account);
    if (account == NULL)
        goto cleanup;

    result = password_verify(account->pass_data, cleartext);

cleanup:
    free(b64d);
    re_free(res);
    free(auth_head);

    return result;
}

static esp_err_t authentication_reject(httpd_req_t *req)
{
    ESP_LOGD(TAG, "authentication_reject");

    esp_err_t ret = httpd_resp_set_hdr(req, http_www_authenticate_hdr, "Basic realm=\"OpenFilPilote\"");
    ESP_LOGV(TAG, "httpd_resp_set_hdr %i", ret);
    if (ret != ESP_OK)
        return ret;

    ret = httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Not Authorized");
    ESP_LOGV(TAG, "httpd_resp_send_err %i", ret);

    return ret;
}

/***************************************************************************/

static bool is_source_ip_authorized(httpd_req_t *req)
{
    ESP_LOGD(TAG, "is_source_ip_authorized");

    const char *ip_filter = CONFIG_OFP_UI_SOURCE_IP_FILTER;
    if (strlen(ip_filter) == 0)
        return true;
    ESP_LOGD(TAG, "Authorized source ip is %s", ip_filter);

    int sockfd = httpd_req_to_sockfd(req);
    char ip_str[INET6_ADDRSTRLEN];
    struct sockaddr_in6 addr;

    socklen_t addr_size = sizeof(addr);
    int n = getpeername(sockfd, (struct sockaddr *)&addr, &addr_size);
    if (n < 0)
    {
        ESP_LOGW(TAG, "getpeername error %i while getting client IP, denying by default", n);
        return false;
    }

#if LWIP_IPV6
    const char *s = inet_ntop(AF_INET6, &addr.sin6_addr, ip_str, sizeof(ip_str));
#else  /* LWIP_IPV6 */
    const char *s = inet_ntop(AF_INET, &addr.sin6_addr.un.u32_addr[3], ip_str, sizeof(ip_str));
#endif /* LWIP_IPV6 */
    if (s == NULL)
    {
        ESP_LOGW(TAG, "Could not get formatted client IP, denying by default");
        return false;
    }

    bool ret = (strcmp(ip_str, ip_filter) == 0);
    ESP_LOGD(TAG, "Request comes from source IP %s, result : %i", ip_str, ret);

    return ret;
}

/***************************************************************************/

static esp_err_t https_handler_generic(httpd_req_t *req)
{
    // check source ip filter
    if (!is_source_ip_authorized(req))
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Source IP not allowed");

#ifdef CONFIG_OFP_UI_WEBSERVER_REQUIRES_AUTHENTICATION
    // check credentials
    if (!is_authentication_valid(req))
        return authentication_reject(req);
#endif

    if (!webserver_is_enabled())
    {
        ESP_LOGW(TAG, "HTTP serving is disabled, skipping request to %s", req->uri);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Service Unavailable");
    }

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

static esp_err_t https_handler_input_middleware(httpd_req_t *req)
{
    // esp_log_level_set(TAG, ESP_LOG_VERBOSE); // DEBUG
    struct timeval begin, end;
    gettimeofday(&begin, NULL);

    esp_err_t result = https_handler_generic(req);

    gettimeofday(&end, NULL);
    uint32_t delta_ms = (end.tv_sec - begin.tv_sec) * 1000LL + (end.tv_usec - begin.tv_usec) / 1000LL;

    const char *m = NULL, *u = "???";
    switch (req->method)
    {
    case HTTP_GET:
        m = http_get;
        break;
    case HTTP_POST:
        m = http_post;
        break;
    case HTTP_PUT:
        m = http_put;
        break;
    case HTTP_PATCH:
        m = http_patch;
        break;
    case HTTP_DELETE:
        m = http_delete;
        break;
    default:
        m = u;
        break;
    }
    ESP_LOGD(TAG, "Request duration for %s %s : %u milliseconds", m, req->uri, delta_ms);

    return result;
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
    webserver_register_wildcard_method(new_server, HTTP_GET, https_handler_input_middleware);
    webserver_register_wildcard_method(new_server, HTTP_POST, https_handler_input_middleware);
    webserver_register_wildcard_method(new_server, HTTP_PUT, https_handler_input_middleware);
    webserver_register_wildcard_method(new_server, HTTP_PATCH, https_handler_input_middleware);
    webserver_register_wildcard_method(new_server, HTTP_DELETE, https_handler_input_middleware);
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

#ifdef CONFIG_OFP_UI_WEBSERVER_REQUIRES_ENCRYPTION
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
#else
    conf.port_insecure = CONFIG_OFP_UI_WEBSERVER_INSECURE_PORT;
    conf.transport_mode = HTTPD_SSL_TRANSPORT_INSECURE;
#endif

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

/* received the required amount of data from incoming request body */
esp_err_t webserver_read_request_data(httpd_req_t *req, char *buf, size_t len)
{
    assert(req != NULL);
    assert(buf != NULL);
    ESP_LOGV(TAG, "Needing %i bytes total", len);

    int remaining = len;
    while (remaining > 0)
    {
        // one block at a time
        ESP_LOGV(TAG, "Requesting %i bytes", remaining);
        int ret = httpd_req_recv(req, buf, remaining);

        // Received some data
        if (ret > 0)
        {
            ESP_LOGV(TAG, "Received %i bytes", ret);
            remaining -= ret;
            buf += ret;
            continue;
        }

        // connection closed
        if (ret == 0)
        {
            ESP_LOGD(TAG, "httpd_req_recv error: connection closed");
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Connection closed");
        }

        // timeout
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            // do not bother retrying
            ESP_LOGD(TAG, "httpd_req_recv error: connection timeout");
            return httpd_resp_send_408(req);
        }

        // In case of error, returning ESP_FAIL will ensure that the underlying socket is closed
        const char *msg = esp_err_to_name(ret);
        ESP_LOGD(TAG, "httpd_req_recv error: %s", msg);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, msg);
    }

    return ESP_OK;
}

/*
 * received the required amount of data from incoming request body
 *
 * Dynamically alloc a buff and ADD NULL terminator
 *
 * Memory MUST BE FREED BY CALLER !
 *
 * Maximum size = CONFIG_OFP_UI_WEBSERVER_DATA_MAX_SIZE_SINGLE_OP
 */
char *webserver_get_request_data_atomic(httpd_req_t *req)
{
    const int max_size = CONFIG_OFP_UI_WEBSERVER_DATA_MAX_SIZE_SINGLE_OP;

    ESP_LOGV(TAG, "Content length %i", req->content_len);

    // include space for a NULL terminator as content is processed as a string
    int needed = req->content_len + 1;
    if (needed > max_size)
    {
        ESP_LOGD(TAG, "Request body (%i) is larger than atomic buffer (%i)", needed, max_size);
        return NULL;
    }

    // alloc
    char *buf = malloc(needed); // add NULL terminator (we process this as string)
    assert(buf != NULL);

    // read
    esp_err_t res = webserver_read_request_data(req, buf, req->content_len);
    if (res != ESP_OK)
    {
        free(buf);
        ESP_LOGD(TAG, "Could not read request data: %s", esp_err_to_name(res));
        return NULL;
    }

    buf[req->content_len] = '\0'; // add NULL terminator

    return buf; // MUST BE FREED BY CALLER
}

/*
 * receive and parse form data from incoming request
 * MUST BE FREED by the caller using form_data_free() from utils.h
 */
struct ofp_form_data *webserver_form_data_from_req(httpd_req_t *req)
{
    char *buf = webserver_get_request_data_atomic(req);
    if (buf == NULL)
    {
        ESP_LOGD(TAG, "Failed getting request data");
        return NULL;
    }

    // dump
    ESP_LOG_BUFFER_HEXDUMP(TAG, buf, req->content_len, ESP_LOG_DEBUG);

    // decode
    struct ofp_form_data *data = form_data_parse(buf);
    free(buf);
    if (data == NULL)
    {
        ESP_LOGD(TAG, "Error parsing x-www-form-urlencoded data");
        return NULL;
    }

    return data;
}