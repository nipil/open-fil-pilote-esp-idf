#include <string.h>
#include <esp_log.h>
#include <mdns.h>

/* menu autoconfig */
#include "sdkconfig.h"

const char mdns_hostname[] = CONFIG_OFP_HOSTNAME;
const char mdns_instance[] = CONFIG_OFP_MDNS_INSTANCE_NAME;

const char TAG_MDNS[] = "m_dns";

void mdns_start()
{
    ESP_LOGI(TAG_MDNS, "Starting MDNS service");

    // initialize mDNS
    ESP_ERROR_CHECK(mdns_init());

    // set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK(mdns_hostname_set(mdns_hostname));
    ESP_LOGI(TAG_MDNS, "mdns hostname set to: [%s]", mdns_hostname);

    // set default mDNS instance name
    ESP_ERROR_CHECK(mdns_instance_name_set(mdns_instance));

    // structure with TXT records
    mdns_txt_item_t serviceTxtData[3] = {
        {"board", "esp32"},
        {"u", "user"},
        {"p", "password"}};

    // initialize service
    ESP_ERROR_CHECK(mdns_service_add(mdns_instance, "_https", "_tcp", 443, serviceTxtData, 3));

    // add another TXT item
    ESP_ERROR_CHECK(mdns_service_txt_item_set("_https", "_tcp", "path", "/"));
}

void mdns_stop()
{
    ESP_LOGI(TAG_MDNS, "Stopping MDNS service");
}