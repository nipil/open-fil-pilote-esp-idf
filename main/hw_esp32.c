#include <stddef.h>
#include <stdio.h>
#include <esp_log.h>
#include "str.h"
#include "hw_esp32.h"

static const char TAG[] = "esp32";

/* defines */
#define HW_ESP32_ZONE_ID_FORMAT "%s%02i"

/* consts */
static const char *str_zone_prefix = "zone_prefix";
static const char *str_zone_count = "zone_count";

/* forward definitions */
static bool hw_esp32_zone_set_init(struct ofp_hw *hw);
static bool hw_esp32_zone_set_apply(struct ofp_hw *hw, struct tm *timeinfo);

/* hardware properties */
static struct ofp_hw hw_esp32 = {
    .id = "ESP32",
    .description = "Any ESP32, to target a new hardware or debug software without special hardware",
    .param_count = 2,
    .params = {
        {
            .id = "zone_count",
            .description = "Nombre de zones",
            .type = HW_OFP_PARAM_TYPE_INTEGER,
            .value = {.int_ = 0},
        },
        {
            .id = "zone_prefix",
            .description = "this is a parameter of type string",
            .type = HW_OFP_PARAM_TYPE_STRING,
            .value = {.string_ = "z"},
        },
    },
    .zone_set = {
        .count = 0,
        .zones = NULL,
    },
    .hw_hooks = {
        .init = hw_esp32_zone_set_init,
        .apply = hw_esp32_zone_set_apply,
    },
};

/* used for registering available hardware */
struct ofp_hw *hw_esp32_get_definition(void)
{
    return &hw_esp32;
}

/* init dynamic data and setup hardware (PLEASE READ ofp_hw_hooks in ofp.h) */
static bool hw_esp32_zone_set_init(struct ofp_hw *hw)
{
    ESP_LOGD(TAG, "hw_esp32_zone_set_init %p", hw);
    assert(hw != NULL);

    // read the zone prefix name
    struct ofp_hw_param *param_zone_prefix = ofp_hw_param_find_by_id(hw, str_zone_prefix);
    assert(param_zone_prefix != NULL);

    // allocate memory for the total number of zones available
    struct ofp_hw_param *param_zone_count = ofp_hw_param_find_by_id(hw, str_zone_count);
    assert(param_zone_count != NULL);
    int zone_count = param_zone_count->value.int_;
    if (!ofp_zone_set_allocate(&hw->zone_set, zone_count))
    {
        ESP_LOGW(TAG, "Could not allocate %i zones", zone_count);
        return false;
    }

    // setup zone names & descriptions
    char buf[OFP_MAX_LEN_VALUE];
    for (int i = 0; i < hw->zone_set.count; i++)
    {
        char *prefix = param_zone_prefix->value.string_;
        int res = snprintf(buf, sizeof(buf), HW_ESP32_ZONE_ID_FORMAT, prefix, i);
        if (res < 0 || res > sizeof(buf))
        {
            ESP_LOGW(TAG, "Zone id too long (prefix %s and number %i)", prefix, i);
            return false;
        }
        struct ofp_zone *zone = &hw->zone_set.zones[i];
        if (!ofp_zone_set_id(zone, buf))
        {
            ESP_LOGW(TAG, "Could not set zone id %s", buf);
            return false;
        }
        if (!ofp_zone_set_description(zone, buf))
        {
            ESP_LOGW(TAG, "Could not set zone description %s", buf);
            return false;
        }
    }

    /*
        INFO: implement hardware initialization here
        Return true if hardware is ready for work
    */
    return true;
}

/* apply dynamic state to hardware */
static bool hw_esp32_zone_set_apply(struct ofp_hw *hw, struct tm *timeinfo)
{
    ESP_LOGD(TAG, "hw_esp32_zone_set_apply %p", hw);
    assert(hw != NULL);
    /*
        INFO: apply zone current state to hardware
        Return true if hardware was successfully updated
    */
    return false;
}