#include <stdio.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "wifi_manager.h"

#include <esp_https_server.h>
#include <esp_http_server.h>

#include "esp_tls.h"
#include "sdkconfig.h"

/* @brief tag used for ESP serial console messages */
const char TAG[] = "main";

/**
 * @brief RTOS task that periodically prints the heap memory available.
 * @note Pure debug information, should not be ever started on production code! This is an example on how you can integrate your code with wifi-manager
 */
void monitoring_task(void *pvParameter)
{
	for (;;)
	{
		ESP_LOGI(TAG, "free heap: %d", esp_get_free_heap_size());
		vTaskDelay(pdMS_TO_TICKS(10000));
	}
}

/* An HTTP GET handler */
esp_err_t root_get_handler(httpd_req_t *req)
{
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, "<h1>Hello Secure World!</h1>", HTTPD_RESP_USE_STRLEN);

	return ESP_OK;
}

/**
 * Example callback function to get the certificate of connected clients,
 * whenever a new SSL connection is created
 *
 * Can also be used to other information like Socket FD, Connection state, etc.
 */
void https_server_user_callback(esp_https_server_user_cb_arg_t *user_cb)
{
	ESP_LOGI(TAG, "Session Created!");
	const mbedtls_x509_crt *cert;

	const size_t buf_size = 1024;
	char *buf = calloc(buf_size, sizeof(char));
	if (buf == NULL)
	{
		ESP_LOGE(TAG, "Out of memory - Callback execution failed!");
		return;
	}

	cert = mbedtls_ssl_get_peer_cert(&user_cb->tls->ssl);
	if (cert != NULL)
	{
		mbedtls_x509_crt_info((char *)buf, buf_size - 1, "      ", cert);
		ESP_LOGI(TAG, "Peer certificate info:\n%s", buf);
	}
	else
	{
		ESP_LOGW(TAG, "Could not obtain the peer certificate!");
	}

	free(buf);
}

httpd_handle_t start_webserver(void)
{
	httpd_handle_t server = NULL;

	// Start the httpd server
	ESP_LOGI(TAG, "Starting server");

	httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();

	extern const unsigned char cacert_pem_start[] asm("_binary_cacert_pem_start");
	extern const unsigned char cacert_pem_end[] asm("_binary_cacert_pem_end");
	conf.cacert_pem = cacert_pem_start;
	conf.cacert_len = cacert_pem_end - cacert_pem_start;

	extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
	extern const unsigned char prvtkey_pem_end[] asm("_binary_prvtkey_pem_end");
	conf.prvtkey_pem = prvtkey_pem_start;
	conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

	conf.user_cb = https_server_user_callback;

	// dedicated webserver control port, for easier coexistance with other servers
	conf.httpd.ctrl_port = 32000;

	// E (9142) httpd: httpd_server_init: error in creating ctrl socket (112)
	esp_err_t ret = httpd_ssl_start(&server, &conf);
	if (ESP_OK != ret)
	{
		ESP_LOGI(TAG, "Error starting server!");
		return NULL;
	}

	// Set URI handlers
	ESP_LOGI(TAG, "Registering URI handlers");

	// Register root URI handler
	const httpd_uri_t root = {
		.uri = "/",
		.method = HTTP_GET,
		.handler = root_get_handler};

	ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root));

	return server;
}

void stop_webserver(httpd_handle_t server)
{
	// Stop the httpd server
	httpd_ssl_stop(server);
}

httpd_handle_t *app_server = NULL;

void cb_connection_ok_handler(void *pvParameter)
{
	ip_event_got_ip_t *param = (ip_event_got_ip_t *)pvParameter;

	/* transform IP to human readable string */
	char str_ip[16];
	esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);

	ESP_LOGI(TAG, "I have a connection and my IP is %s!", str_ip);
	if (app_server == NULL)
	{
		app_server = start_webserver();
	}
}

void cb_disconnect_handler(void *pvParameter)
{
	ESP_LOGI(TAG, "STA Disconnected");
	if (app_server)
	{
		stop_webserver(app_server);
		app_server = NULL;
	}
}

void app_main()
{
	/* start the wifi manager */
	wifi_manager_start();

	/* register a callback as an example to how you can integrate your code with the wifi manager */
	wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok_handler);
	wifi_manager_set_callback(WM_EVENT_STA_DISCONNECTED, &cb_disconnect_handler);

	/* your code should go here. Here we simply create a task on core 2 that monitors free heap memory */
	xTaskCreatePinnedToCore(&monitoring_task, "monitoring_task", 2048, NULL, 1, NULL, 1);
}