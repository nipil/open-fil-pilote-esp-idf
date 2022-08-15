#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_netif.h>

/* wifi manager */
#include "wifi_manager.h"

/* menu autoconfig */
#include "sdkconfig.h"

#include "uptime.h"
#include "utils.h"
#include "sntp.h"
#include "webserver.h"

/* @brief TAG_MAIN used for ESP serial console messages */
const char TAG_MAIN[] = "main";

/***************************************************************************/

void display_ip(ip_event_got_ip_t *param, char *msg)
{
	char str_ip[16];
	esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);
	ESP_LOGI(TAG_MAIN, "%s : %s", msg, str_ip);
}

void wifi_manager_connected_callback(void *pvParameter)
{
	display_ip((ip_event_got_ip_t *)pvParameter, "STA Connected. IP is");
	webserver_start();
	sntp_task_start();
	ESP_LOGI(TAG_MAIN, "Connection processing finished.");
}

void wifi_manager_disconnected_callback(void *pvParameter)
{
	ESP_LOGI(TAG_MAIN, "STA Disconnected");
	webserver_stop();
	sntp_task_stop();
	ESP_LOGI(TAG_MAIN, "Disconnection processing finished.");
}

/***************************************************************************/

void app_main()
{
	// compensate uptime according to clock leap from SNTP
	uptime_sync_start();

	/* start the wifi manager */
	wifi_manager_start();

	/* register a callback as an example to how you can integrate your code with the wifi manager */
	wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &wifi_manager_connected_callback);
	wifi_manager_set_callback(WM_EVENT_STA_DISCONNECTED, &wifi_manager_disconnected_callback);
}