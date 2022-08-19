#include <stdio.h>
#include <esp_log.h>
#include <esp_netif.h>

/* wifi manager */
#include "wifi_manager.h"

/* menu autoconfig */
#include "sdkconfig.h"

#include "ofp.h"
#include "uptime.h"
#include "utils.h"
#include "sntp.h"
#include "webserver.h"
#include "m_dns.h"

// hardware
#include "hw_esp32.h"
#include "hw_m1e1.h"

static const char TAG[] = "main";

/***************************************************************************/

static void display_ip(ip_event_got_ip_t *param, char *msg)
{
	char str_ip[16];
	esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);
	ESP_LOGI(TAG, "%s : %s", msg, str_ip);
}

static void wifi_manager_connected_callback(void *pvParameter)
{
	display_ip((ip_event_got_ip_t *)pvParameter, "STA Connected. IP is");
	mdns_start();
	webserver_start();
	sntp_task_start();
	ESP_LOGI(TAG, "Connection processing finished.");
}

static void wifi_manager_disconnected_callback(void *pvParameter)
{
	ESP_LOGI(TAG, "STA Disconnected");
	mdns_stop();
	webserver_stop();
	sntp_task_stop();
	ESP_LOGI(TAG, "Disconnection processing finished.");
}

/***************************************************************************/

static void register_hardware(void)
{
	ofp_hw_register(hw_esp32_get_definition());
	ofp_hw_register(hw_m1e1_get_definition());
}

/***************************************************************************/

void app_main()
{
	// compensate uptime according to clock leap from SNTP
	uptime_sync_start();

	// register every hardware available at compilation time
	register_hardware();

	/* start the wifi manager */
	wifi_manager_start();

	/* register a callback as an example to how you can integrate your code with the wifi manager */
	wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &wifi_manager_connected_callback);
	wifi_manager_set_callback(WM_EVENT_STA_DISCONNECTED, &wifi_manager_disconnected_callback);
}