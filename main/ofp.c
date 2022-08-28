#include <string.h>

#include <esp_log.h>

#include "str.h"
#include "ofp.h"
#include "storage.h"
#include "api_hw.h"

static const char TAG[] = "ofp";

/* defines */
#define DEFAULT_FIXED_ORDER_FOR_ZONES HW_OFP_ORDER_ID_STANDARD_COZY

/* constants */
static const char str_re_zone_config_mode_value[] = "^m([[:digit:]]+):v([[:digit:]]+)^";

/* global hardware instance, get it using ofp_hw_get() */
static struct ofp_hw *hw_global = NULL;

/* where the registered hardware are stored */
static struct ofp_hw_list hw_list = {.hw_count = 0, .hw = {NULL}};

/* const definition */
static const struct ofp_order_info order_info[] = {
    {.id = "offload",
     .name = "Arr&ecirc;t / D&eacute;lestage",
     .class = "secondary"},
    {.id = "nofreeze",
     .name = "Hors-gel",
     .class = "info"},
    {.id = "economy",
     .name = "Economie",
     .class = "success"},
    {.id = "cozy",
     .name = "Confort",
     .class = "danger"},
    {.id = "cozyminus2",
     .name = "Confort-2&deg;",
     .class = "warning"},
    {.id = "cozyminus1",
     .name = "Confort-1&deg;",
     .class = "warning"}};

/*
 * Make a hardware available to the system.
 *
 * Stores the pointer to its definition in the first available slot
 */
void ofp_hw_register(struct ofp_hw *hw)
{
    assert(hw);
    assert(hw_list.hw_count < OFP_MAX_SIMULTANEOUS_HARDWARE);

    ESP_LOGD(TAG, "Registering hardware definition %s with %i parameters", hw->id, hw->param_count);
    hw_list.hw[hw_list.hw_count++] = hw;
}

/*
 * Get hardware by index
 */
int ofp_hw_list_get_count(void)
{
    return hw_list.hw_count;
}

struct ofp_hw *ofp_hw_list_get_hw_by_index(int n)
{
    assert(n < hw_list.hw_count);
    assert(n >= 0);
    return hw_list.hw[n];
}

/*
 * Search for a specific hardware
 */
struct ofp_hw *ofp_hw_list_find_hw_by_id(char *hw_id)
{
    assert(hw_id);
    for (int i = 0; i < hw_list.hw_count; i++)
    {
        struct ofp_hw *cur = hw_list.hw[i];
        if (strcmp(cur->id, hw_id) == 0)
        {
            return cur;
        }
    }
    return NULL;
}

struct ofp_hw_param *ofp_hw_param_find_by_id(struct ofp_hw *hw, const char *param_id)
{
    assert(hw != NULL);
    assert(param_id != NULL);
    for (int i = 0; i < hw->param_count; i++)
    {
        struct ofp_hw_param *cur = &hw->params[i];
        if (strcmp(cur->id, param_id) == 0)
            return cur;
    }
    return NULL;
}

bool ofp_hw_param_set_value_string(struct ofp_hw_param *param, const char *str)
{
    assert(param != NULL);
    assert(str != NULL);

    int len = strlen(str);
    if (len + 1 > OFP_MAX_LEN_VALUE)
    {
        ESP_LOGW(TAG, "String too large for hardware parameter %s", param->id);
        return false;
    }

    strcpy(param->value.string_, str);
    return true;
}

static bool ofp_hw_param_load_value_integer(const char *hw_id, struct ofp_hw_param *param)
{
    kvh_get(param->value.int_, i32, kv_get_ns_hardware(), param->id, param->value.int_);
    return true;
}

static bool ofp_hw_param_load_value_string(const char *hw_id, struct ofp_hw_param *param)
{
    char *buf;
    kvh_get(buf, str, kv_get_ns_hardware(), param->id); // result MUST BE FREED by caller

    // keeping defaults is not an error
    if (buf == NULL)
        return true;

    // restoring
    if (ofp_hw_param_set_value_string(param, buf))
    {
        free(buf);
        return true;
    }

    return false;
}

static bool ofp_hw_param_load_value(const char *hw_id, struct ofp_hw_param *param)
{
    bool result;
    ESP_LOGV(TAG, "hardware param %s", param->id);

    switch (param->type)
    {
    case HW_OFP_PARAM_TYPE_INTEGER:
        result = ofp_hw_param_load_value_integer(hw_id, param);
        ESP_LOGV(TAG, "value integer %i", param->value.int_);
        return result;

    case HW_OFP_PARAM_TYPE_STRING:
        result = ofp_hw_param_load_value_string(hw_id, param);
        ESP_LOGV(TAG, "value string %s", param->value.string_);
        return result;

    default:
        ESP_LOGW(TAG, "Invalid parameter type detected: %i", param->type);
        return false;
    }
}

bool ofp_zone_set_id(struct ofp_zone *zone, const char *id)
{
    assert(zone != NULL);
    assert(id != NULL);

    int len = strlen(id);
    if (len + 1 > OFP_MAX_LEN_ID)
        return false;

    strcpy(zone->id, id);
    return true;
}

bool ofp_zone_set_description(struct ofp_zone *zone, const char *description)
{
    assert(zone != NULL);
    assert(description != NULL);

    int len = strlen(description);
    if (len + 1 > OFP_MAX_LEN_DESCRIPTION)
        return false;

    strcpy(zone->description, description);
    return true;
}

bool ofp_zone_set_mode_fixed(struct ofp_zone *zone, enum ofp_order_id order_id)
{
    if (!ofp_order_id_is_valid(order_id))
    {
        ESP_LOGW(TAG, "Invalid ofp_order_id value (%i)", order_id);
        return false;
    }
    zone->mode = HW_OFP_ZONE_MODE_FIXED;
    zone->mode_data.order_id = order_id;
    ESP_LOGV(TAG, "zone %s mode %i order_id %i", zone->id, zone->mode, zone->mode_data.order_id);
    return true;
}

bool ofp_zone_set_mode_planning(struct ofp_zone *zone, int planning_id)
{
    if (!ofp_planning_id_is_valid(planning_id))
    {
        ESP_LOGW(TAG, "Invalid planning_id value (%i)", planning_id);
        return false;
    }
    zone->mode = HW_OFP_ZONE_MODE_PLANNING;
    zone->mode_data.planning_id = planning_id;
    ESP_LOGV(TAG, "zone %s mode %i order_id %i", zone->id, zone->mode, zone->mode_data.planning_id);
    return true;
}

/* generic function to set the zone mode */
static bool ofp_zone_set_mode(struct ofp_zone *zone, enum ofp_zone_mode mode, int value)
{
    switch (mode)
    {
    case HW_OFP_ZONE_MODE_FIXED:
        return ofp_zone_set_mode_fixed(zone, value);
        break;
    case HW_OFP_ZONE_MODE_PLANNING:
        return ofp_zone_set_mode_planning(zone, value);
    default:
        ESP_LOGW(TAG, "Invalid ofp_zone_mode value (%i)", mode);
        return false;
    }
}

/* load zone configuration from NVS */
static bool ofp_zone_load_mode(const char *hw_id, struct ofp_zone *zone)
{
    // TODO: test after zone config write from API is implemented
    ESP_LOGV(TAG, "zone id %s desc %s", zone->id, zone->description);

    // defaults
    zone->mode = HW_OFP_ZONE_MODE_FIXED;
    zone->mode_data.order_id = DEFAULT_FIXED_ORDER_FOR_ZONES;

    // initialize to safe value
    // will be overwritten by ofp_zone_update_current
    zone->current = DEFAULT_FIXED_ORDER_FOR_ZONES;

    // fetch
    char *buf;
    kvh_get(buf, str, kv_get_ns_zone(), zone->id); // result MUST BE FREED by caller
    if (buf == NULL)
    {
        ESP_LOGV(TAG, "Could not get stored mode-string");
        return false;
    }

    // parse
    int result = true;
    ESP_LOGV(TAG, "Zone config string: %s", buf);
    struct re_result *res = re_match(str_re_zone_config_mode_value, buf);
    if (res == NULL || res->count != 3 || !ofp_zone_set_mode(zone, atoi(res->strings[1]), atoi(res->strings[2])))
    {
        ESP_LOGW(TAG, "Invalid mode-string %s for zone %s", buf, zone->id);
        result = false;
    }

    // cleanup
    if (res != NULL)
        re_free(res);
    free(buf);

    return result;
}

const struct ofp_order_info *ofp_order_info_by_num_id(enum ofp_order_id order_id)
{
    assert(order_id < HW_OFP_ORDER_ID_ENUM_SIZE);
    return &order_info[order_id];
}

const struct ofp_order_info *ofp_order_info_by_str_id(char *order_id)
{
    assert(order_id != NULL);
    for (int i = 0; i < HW_OFP_ORDER_ID_ENUM_SIZE; i++)
        if (strcmp(order_id, order_info[i].id) == 0)
            return &order_info[i];
    return NULL;
}

bool ofp_order_id_is_valid(enum ofp_order_id order_id)
{
    return (order_id >= HW_OFP_ORDER_ID_STANDARD_OFFLOAD && order_id < HW_OFP_ORDER_ID_ENUM_SIZE);
}

/* allocate new space for zones set */
bool ofp_zone_set_allocate(struct ofp_zone_set *zone_set, int zone_count)
{
    ESP_LOGD(TAG, "ofp_zone_set_allocate %p %i", zone_set, zone_count);
    assert(zone_set != NULL);

    if (zone_count > OFP_MAX_ZONE_COUNT)
    {
        ESP_LOGW(TAG, "Trying to allocate too many zones (%i) while maximum is %i", zone_count, OFP_MAX_ZONE_COUNT);
        return false;
    }

    // empty
    if (zone_count == 0)
    {
        zone_set->count = 0;
        zone_set->zones = NULL;
        return true;
    }

    // non empty
    struct ofp_zone *buf = malloc(zone_count * sizeof(struct ofp_zone));
    assert(buf != NULL);
    zone_set->count = zone_count;
    zone_set->zones = buf;

    return true;
}

/* initialize the hardware based on stored hardware id */
void ofp_hw_initialize(void)
{
    ESP_LOGD(TAG, "ofp_hw_initialize");

    // get selected hardware id from storage
    struct ofp_hw *current_hw = ofp_get_hardware_from_stored_id();
    ESP_LOGD(TAG, "stored hardware %p", current_hw);
    if (current_hw == NULL)
    {
        ESP_LOGW(TAG, "No hardware selected, disabling hardware");
        return;
    }

    ESP_LOGI(TAG, "Initializing hardware %s", current_hw->id);

    if (!kv_set_ns_current_hardware(current_hw->id))
    {
        ESP_LOGE(TAG, "Could not set hardware namespace, disabling hardware");
        return;
    }

    // load valid saved hardware parameters from storage
    for (int i = 0; i < current_hw->param_count; i++)
    {
        ESP_LOGV(TAG, "parameter %i", i);
        struct ofp_hw_param *param = &current_hw->params[i];
        if (!ofp_hw_param_load_value(current_hw->id, param))
        {
            ESP_LOGW(TAG, "Could not load value for hardware parameter %s, reverting to default", param->id);
            continue;
        }
    }

    // initialize
    if (!current_hw->hw_hooks.init(current_hw))
    {
        ESP_LOGE(TAG, "Could not initialize hardware %s, disabling hardware", current_hw->id);
        return;
    }

    if (!kv_set_ns_current_zone(current_hw->id))
    {
        ESP_LOGE(TAG, "Could not set zone namespace, disabling hardware");
        return;
    }

    // load valid saved state from storage
    ESP_LOGI(TAG, "Loading zones configuration for hardware %s", current_hw->id);
    for (int i = 0; i < current_hw->zone_set.count; i++)
    {
        ESP_LOGV(TAG, "zone %i", i);
        struct ofp_zone *zone = &current_hw->zone_set.zones[i];
        if (!ofp_zone_load_mode(current_hw->id, zone))
        {
            ESP_LOGW(TAG, "Could not load configuration for zone %s, reverting to default", zone->id);
            continue;
        }
    }

    // persist reference
    hw_global = current_hw;
}

static bool ofp_zone_update_current(struct ofp_zone *zone)
{
    ESP_LOGD(TAG, "ofp_zone_update_current");

    return false;
}

void ofp_hw_update(struct ofp_hw *hw)
{
    ESP_LOGD(TAG, "ofp_hw_update_zones");

    // current time
    time_t now;
    struct tm ti;
    time(&now);
    time_to_localtime(&now, &ti);

    char buf[64];
    localtime_to_string(&ti, buf, 64);
    ESP_LOGV(TAG, "current time: %s", buf);

    // compute current zone orders
    for (int i = 0; i < hw->zone_set.count; i++)
    {
        struct ofp_zone *zone = &hw->zone_set.zones[i];
        // TODO: get plannings, find timeslot, get final order, update current
        if (!ofp_zone_update_current(zone))
        {
            ESP_LOGW(TAG, "Could not update zone %s current order", zone->id);
            continue;
        }
        ESP_LOGV(TAG, "zone %s current %i", zone->id, zone->current);
    }

    // apply resulting zone orders to the hardware
    hw->hw_hooks.apply(hw);
}

/* access the global hardware instance */
struct ofp_hw *ofp_hw_get_current(void)
{
    return hw_global;
}

bool ofp_planning_id_is_valid(int planning_id)
{
    // TODO: implement
    return false;
}