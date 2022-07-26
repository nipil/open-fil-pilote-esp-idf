#include <stdio.h>
#include <esp_log.h>
#include <esp_netif.h>

/* wifi manager */
#include "wifi_manager.h"

/* menu autoconfig */
#include "sdkconfig.h"

#include "str.h"
#include "ofp.h"
#include "uptime.h"
#include "utils.h"
#include "sntp.h"
#include "webserver.h"
#include "m_dns.h"
#include "storage.h"
#include "console.h"
#include "fwupd.h"

// hardware
#include "hw_esp32.h"
#include "hw_m1e1.h"

static const char TAG[] = "main";

/* defines */
#define MAIN_LOOP_WAIT_MILLISECONDS (1000)

/***************************************************************************/

static void display_ip(ip_event_got_ip_t *param, char *msg)
{
    char str_ip[16];
    esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);
    ESP_LOGI(TAG, "%s : %s", msg, str_ip);
}

static void wifi_manager_attempt_callback(void *pvParameter)
{
    ESP_LOGD(TAG, "STA associated");
    uptime_track_wifi_attempt();
    ESP_LOGV(TAG, "Association processing finished.");
}

static void wifi_manager_connected_callback(void *pvParameter)
{
    display_ip((ip_event_got_ip_t *)pvParameter, "STA Connected. IP is");
    uptime_track_wifi_success();
    mdns_start();
    webserver_start();
    sntp_task_start();
    ESP_LOGV(TAG, "Connection processing finished.");
}

static void wifi_manager_disconnected_callback(void *pvParameter)
{
    ESP_LOGI(TAG, "STA Disconnected");
    uptime_track_wifi_disconnect();
    mdns_stop();
    webserver_stop();
    sntp_task_stop();
    ESP_LOGV(TAG, "Disconnection processing finished.");
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
    // use default partition for NVS content
    kv_init(NULL);

    // initialize accounts
    ofp_account_list_init();

    // compensate uptime according to clock leap from SNTP
    uptime_sync_start();

    // initialize plannings
    ofp_override_load();

    // initialize plannings
    ofp_planning_list_init();

    // register every hardware available at compilation time
    register_hardware();

    // initialize global hardware reference if successful
    ofp_hw_initialize();

    // initialize terminal
    console_init();

    // confirm OTA update if pending
    fwupd_confirm();

#ifndef OFP_DISABLE_NETWORKING
    /* start the wifi manager */
    wifi_manager_start();

    /* register a callback as an example to how you can integrate your code with the wifi manager */
    wifi_manager_set_callback(WM_ORDER_CONNECT_STA, &wifi_manager_attempt_callback);
    wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &wifi_manager_connected_callback);
    wifi_manager_set_callback(WM_EVENT_STA_DISCONNECTED, &wifi_manager_disconnected_callback);
#endif /* OFP_NO_NETWORKING */

    // use global hardware reference
    struct ofp_hw *current_hw = ofp_hw_get_current();

    /* main loop */
    while (current_hw != NULL)
    {
        // current time
        time_t now;
        struct tm ti;
        time(&now);
        time_to_localtime(&now, &ti);

        // track time
        char buf[LOCALTIME_TO_STRING_BUFFER_LENGTH];
        localtime_to_string(&ti, buf, sizeof(buf));
        ESP_LOGV(TAG, "current time: %s", buf);

        // compute orders
        ofp_zone_update_current_orders(current_hw, &ti);
        // apply orders
        current_hw->hw_hooks.apply(current_hw, &ti);

        // sleep
        vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_WAIT_MILLISECONDS));
    }
    ESP_LOGD(TAG, "app_main finished");
}
