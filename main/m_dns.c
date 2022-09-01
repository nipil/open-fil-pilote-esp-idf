#include <string.h>
#include <esp_log.h>
#include <mdns.h>

/* menu autoconfig */
#include "sdkconfig.h"

#include "str.h"

static const char mdns_hostname[] = CONFIG_OFP_HOSTNAME;
static const char mdns_instance[] = CONFIG_OFP_MDNS_INSTANCE_NAME;

static const char TAG[] = "m_dns";

void mdns_start(void)
{
    ESP_LOGI(TAG, "Starting MDNS service");

    // initialize mDNS
    ESP_ERROR_CHECK(mdns_init());

    // set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK(mdns_hostname_set(mdns_hostname));
    ESP_LOGI(TAG, "MDNS hostname: %s.local", mdns_hostname);

    // set default mDNS instance name
    ESP_ERROR_CHECK(mdns_instance_name_set(mdns_instance));

    // structure with TXT records
    mdns_txt_item_t serviceTxtData[3] = {
        {"board", "esp32"},
        {"u", "user"},
        {"p", "password"}};

    // initialize service
    ESP_ERROR_CHECK(mdns_service_add(mdns_instance, "_https", "_tcp", 443, serviceTxtData, 3));

    /*
     * TODO: investiguer
     * A crash caught after a wifi reconnection
     *
    I (16:35:38.941) m_dns: MDNS hostname: openfilpilote.local
    ESP_ERROR_CHECK failed: esp_err_t 0x102 (ESP_ERR_INVALID_ARG) at 0x40089370
    0x40089370: _esp_error_check_failed at C:/esp/esp-idf/components/esp_system/esp_err.c:42

    file: "./main/m_dns.c" line 36
    func: mdns_start
    expression: mdns_service_add(mdns_instance, "_https", "_tcp", 443, serviceTxtData, 3)

    abort() was called at PC 0x40089373 on core 0
    */

    // add another TXT item
    ESP_ERROR_CHECK(mdns_service_txt_item_set("_https", "_tcp", "path", "/"));
}

void mdns_stop(void)
{
    ESP_LOGI(TAG, "Stopping MDNS service");
}