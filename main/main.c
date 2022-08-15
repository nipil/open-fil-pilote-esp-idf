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

void wifi_manager_connected_callback(void *pvParameter)
{
	ip_event_got_ip_t *param = (ip_event_got_ip_t *)pvParameter;

	/* transform IP to human readable string */
	char str_ip[16];
	esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);
	ESP_LOGI(TAG_MAIN, "I have a connection and my IP is %s!", str_ip);

	if (!webserver_start())
	{
		ESP_LOGE(TAG_MAIN, "Failed to start webserver");
		return;
	}

	sntp_task_start();
}

void wifi_manager_disconnected_callback(void *pvParameter)
{
	ESP_LOGI(TAG_MAIN, "STA Disconnected");
	webserver_stop();
	sntp_task_stop();
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