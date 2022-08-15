#include <stdio.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

/* wifi manager */
#include "wifi_manager.h"
#include "http_app.h"

#include <esp_https_server.h>
#include <esp_http_server.h>
#include <esp_sntp.h>

#include "esp_tls.h"

/* authorisation HTTP 401 */
#include "httpd_basic_auth.h"

/* menu autoconfig */
#include "sdkconfig.h"

#include "uptime.h"

// #define TLS_REQ_CLIENT_CERT

/* @brief TAG_MAIN used for ESP serial console messages */
const char TAG_MAIN[] = "main";

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

	ESP_LOGI(TAG_MAIN, "%i bytes in HTML", ofp_html_len);
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *) ofp_html_start, ofp_html_len);

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

#ifdef TLS_REQ_CLIENT_CERT
/**
 * Example callback function to get the certificate of connected clients,
 * whenever a new SSL connection is created
 *
 * Can also be used to other information like Socket FD, Connection state, etc.
 */
void https_server_user_callback(esp_https_server_user_cb_arg_t *user_cb)
{
	ESP_LOGI(TAG_MAIN, "Session Created!");
	const mbedtls_x509_crt *cert;

	const size_t buf_size = 1024;
	char *buf = calloc(buf_size, sizeof(char));
	if (buf == NULL)
	{
		ESP_LOGE(TAG_MAIN, "Out of memory - Callback execution failed!");
		return;
	}

	cert = mbedtls_ssl_get_peer_cert(&user_cb->tls->ssl);
	if (cert != NULL)
	{
		mbedtls_x509_crt_info((char *)buf, buf_size - 1, "      ", cert);
		ESP_LOGI(TAG_MAIN, "Peer certificate info:\n%s", buf);
	}
	else
	{
		ESP_LOGW(TAG_MAIN, "Could not obtain the peer certificate!");
	}

	free(buf);
}

#endif // TLS_REQ_CLIENT_CERT

httpd_handle_t start_webserver(void)
{
	httpd_handle_t server = NULL;

	// Start the httpd server
	ESP_LOGI(TAG_MAIN, "Starting server");

	httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();

	extern const unsigned char cacert_pem_start[] asm("_binary_cacert_pem_start");
	extern const unsigned char cacert_pem_end[] asm("_binary_cacert_pem_end");
	conf.cacert_pem = cacert_pem_start;
	conf.cacert_len = cacert_pem_end - cacert_pem_start;

	extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
	extern const unsigned char prvtkey_pem_end[] asm("_binary_prvtkey_pem_end");
	conf.prvtkey_pem = prvtkey_pem_start;
	conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

#ifdef TLS_REQ_CLIENT_CERT
	conf.user_cb = https_server_user_callback;
#endif // TLS_REQ_CLIENT_CERT

	// dedicated webserver control port, for easier coexistance with other servers
	conf.httpd.ctrl_port = 32000;

	// E (9142) httpd: httpd_server_init: error in creating ctrl socket (112)
	esp_err_t ret = httpd_ssl_start(&server, &conf);
	if (ESP_OK != ret)
	{
		ESP_LOGI(TAG_MAIN, "Error starting server!");
		return NULL;
	}

	// Set URI handlers
	ESP_LOGI(TAG_MAIN, "Registering URI handlers");

	// Register root URI handler
	const httpd_uri_t root = {
		.uri = "/",
		.method = HTTP_GET,
		.handler = https_handler_root};

	ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root));

	const httpd_uri_t private = {
		.uri = "/private",
		.method = HTTP_GET,
		.handler = https_handler_private};

	ESP_ERROR_CHECK(httpd_register_uri_handler(server, &private));

	return server;
}

void stop_webserver(httpd_handle_t server)
{
	// Stop the httpd server
	httpd_ssl_stop(server);
}

/***************************************************************************/

/* converts time to localtime using timezone */
void time_to_localtime(time_t *val, struct tm *timeinfo)
{
	setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); // TODO: migrate TZ definition to menuconfig
	tzset();
	localtime_r(val, timeinfo);
}

/* format localtime */
void localtime_to_string(struct tm *timeinfo, char *buf, int buf_len)
{
	strftime(buf, buf_len, "%c %z %Z", timeinfo);
}

void sntp_callback(struct timeval *tv)
{
	time_t now;
	struct tm timeinfo;
	char buf[40];

	/* notify uptime handler ASAP to handle possible clock jump */
	uptime_sync_check();

	/* track SNTP messages */
	ESP_LOGI(TAG_MAIN, "SNTP received time since epoch : = %jd sec: %li ms: %li us",
			 (intmax_t)tv->tv_sec,
			 tv->tv_usec / 1000,
			 tv->tv_usec % 1000);

	/* log new localized time */
	time(&now);
	time_to_localtime(&now, &timeinfo);
	localtime_to_string(&timeinfo, buf, sizeof(buf));
	ESP_LOGI(TAG_MAIN, "Current time is : %s", buf);
}

/***************************************************************************/

void cb_connection_ok_handler(void *pvParameter)
{
	ip_event_got_ip_t *param = (ip_event_got_ip_t *)pvParameter;

	/* transform IP to human readable string */
	char str_ip[16];
	esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);
	ESP_LOGI(TAG_MAIN, "I have a connection and my IP is %s!", str_ip);

	/* start ou own HTTPS webserver */
	if (app_server == NULL)
	{
		app_server = start_webserver();
	}

	/* start network time synchronization */
	sntp_set_time_sync_notification_cb(sntp_callback);
	sntp_setservername(0, "pool.ntp.org");
	ESP_LOGI(TAG_MAIN, "Starting SNTP time configuration!");
	sntp_init();
}

void cb_disconnect_handler(void *pvParameter)
{
	ESP_LOGI(TAG_MAIN, "STA Disconnected");

	if (app_server)
	{
		stop_webserver(app_server);
		app_server = NULL;
	}

	ESP_LOGI(TAG_MAIN, "Stopping SNTP synchronization");
	sntp_stop();
}

/***************************************************************************/

void app_main()
{
	// compensate uptime according to clock leap from SNTP
	uptime_sync_start();

	/* start the wifi manager */
	wifi_manager_start();

	/* register a callback as an example to how you can integrate your code with the wifi manager */
	wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok_handler);
	wifi_manager_set_callback(WM_EVENT_STA_DISCONNECTED, &cb_disconnect_handler);
}