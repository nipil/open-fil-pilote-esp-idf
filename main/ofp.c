#include <string.h>

#include <esp_log.h>

#include "str.h"
#include "ofp.h"
#include "storage.h"
#include "api_hw.h"

static const char TAG[] = "ofp";

/* defines */
#define DEFAULT_FIXED_ORDER_FOR_ZONES HW_OFP_ORDER_ID_STANDARD_COZY
#define OFP_MAX_LEN_PLANNING_SLOT_VALUE 10 // dow:hour:minute:order_id

/* constants */
static const char str_zone_config_mode_value_regex[] = "^([[:digit:]]+):([[:digit:]]+):(.*)$"; // mode:value:description
static const char str_zone_config_mode_value_printf[] = "%i:%i:%s";
static const char str_planning_slot_value_printf[] = "d%ih%im%io%i";
static const char str_planning_slot_value_regex[] = "^([[:digit:]]+):(2[0-3]|[0-1][[:digit:]]):([0-5][[:digit:]]):([[:digit:]]+)$"; // dow:hour:minute:order_id

/* global override instance */
static struct ofp_override override_global = {
    .active = false,
    .order_id = DEFAULT_FIXED_ORDER_FOR_ZONES,
};

/* global hardware instance, get it using ofp_hw_get() */
static struct ofp_hw *hw_global = NULL;

/* global planning instance, get it using ofp_planning_list_get() */
struct ofp_planning_list *plan_list_global = NULL;

/* where the registered hardware are stored */
static struct ofp_hw_list hw_list = {.hw_count = 0, .hw = {NULL}};

/* const definition */
static const struct ofp_order_info order_info[] = {
    {
        .id = "offload",
        .name = "Arr&ecirc;t / D&eacute;lestage",
        .class = "secondary",
        .order_id = HW_OFP_ORDER_ID_STANDARD_OFFLOAD,
    },
    {
        .id = "nofreeze",
        .name = "Hors-gel",
        .class = "info",
        .order_id = HW_OFP_ORDER_ID_STANDARD_NOFREEZE,
    },
    {
        .id = "economy",
        .name = "Economie",
        .class = "success",
        .order_id = HW_OFP_ORDER_ID_STANDARD_ECONOMY,
    },
    {
        .id = "cozy",
        .name = "Confort",
        .class = "danger",
        .order_id = HW_OFP_ORDER_ID_STANDARD_COZY,
    },
    {
        .id = "cozyminus2",
        .name = "Confort-2&deg;",
        .class = "warning",
        .order_id = HW_OFP_ORDER_ID_EXTENDED_COZYMINUS2,
    },
    {
        .id = "cozyminus1",
        .name = "Confort-1&deg;",
        .class = "warning",
        .order_id = HW_OFP_ORDER_ID_EXTENDED_COZYMINUS1,
    }};

struct ofp_day_of_week_info day_of_week_info[] = {
    {
        OFP_DOW_SUNDAY,
        "Dimanche",
    },
    {
        OFP_DOW_MONDAY,
        "Lundi",
    },
    {
        OPF_DOW_TUESDAY,
        "Mardi",
    },
    {
        OFP_DOW_WEDNESDAY,
        "Mercredi",
    },
    {
        OFP_DOW_THURSDAY,
        "Jeudi",
    },
    {
        OFP_DOW_FRIDAY,
        "Vendredi",
    },
    {
        OFP_DOW_SATURDAY,
        "Samedi",
    },
};

/* day_of_week_info */

bool ofp_day_of_week_is_valid(enum ofp_day_of_week dow)
{
    return (dow >= 0 && dow < OFP_DOW_ENUM_SIZE);
}

struct ofp_day_of_week_info *ofp_day_of_week_info_find_by_name(const char *name)
{
    if (name == NULL)
        return NULL;
    for (int i = 0; i < OFP_DOW_ENUM_SIZE; i++)
    {
        struct ofp_day_of_week_info *info = &day_of_week_info[i];
        if (strcmp(info->name, name) == 0)
            return info;
    }
    return NULL;
}

struct ofp_day_of_week_info *ofp_day_of_week_info_find_by_id(enum ofp_day_of_week dow)
{
    if (!ofp_day_of_week_is_valid(dow))
        return NULL;
    for (int i = 0; i < OFP_DOW_ENUM_SIZE; i++)
    {
        struct ofp_day_of_week_info *info = &day_of_week_info[i];
        if (info->id == dow)
            return info;
    }
    return NULL;
}

/* override */
void ofp_override_load(void)
{
    ESP_LOGD(TAG, "ofp_override_load");
    int32_t n = kv_ns_get_i32_atomic(kv_get_ns_ofp(), stor_key_zone_override, -1);
    if (n == -1)
    {
        ofp_override_disable();
        return;
    }
    ofp_override_enable(n);
}

void ofp_override_store(void)
{
    ESP_LOGD(TAG, "ofp_override_store");

    if (!override_global.active)
    {
        kv_ns_delete_atomic(kv_get_ns_ofp(), stor_key_zone_override);
        return;
    }

    kv_ns_set_i32_atomic(kv_get_ns_ofp(), stor_key_zone_override, override_global.order_id);
}

void ofp_override_enable(enum ofp_order_id order_id)
{
    ESP_LOGD(TAG, "ofp_override_enable order_id %i", order_id);
    assert(ofp_order_id_is_valid(order_id));
    override_global.active = true;
    override_global.order_id = order_id;
}

void ofp_override_disable(void)
{
    ESP_LOGD(TAG, "ofp_override_disable");
    override_global.active = false;
    override_global.order_id = DEFAULT_FIXED_ORDER_FOR_ZONES;
}

bool ofp_override_get_order_id(enum ofp_order_id *order_id)
{
    assert(order_id != NULL);

    if (!override_global.active)
        return false;

    *order_id = override_global.order_id;
    return true;
}

/* private forward declarations */
static void ofp_planning_list_load_plannings(void);

/*
 * Make a hardware available to the system.
 *
 * Stores the pointer to its definition in the first available slot
 */
void ofp_hw_register(struct ofp_hw *hw)
{
    assert(hw);
    assert(hw_list.hw_count < OFP_MAX_HARDWARE_COUNT);

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
    param->value.int_ = kv_ns_get_i32_atomic(kv_get_ns_hardware(), param->id, param->value.int_);
    return true;
}

static bool ofp_hw_param_load_value_string(const char *hw_id, struct ofp_hw_param *param)
{
    char *buf = kv_ns_get_str_atomic(kv_get_ns_hardware(), param->id); // result MUST BE FREED by caller

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
    if (ofp_planning_list_find_planning_by_id(planning_id) == NULL)
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
    assert(hw_id != NULL);
    assert(zone != NULL);

    ESP_LOGD(TAG, "ofp_zone_load_mode zone id %s desc %s", zone->id, zone->description);

    // defaults
    zone->mode = HW_OFP_ZONE_MODE_FIXED;
    zone->mode_data.order_id = DEFAULT_FIXED_ORDER_FOR_ZONES;

    // initialize to safe value
    // will be overwritten by ofp_zone_update_current
    zone->current = DEFAULT_FIXED_ORDER_FOR_ZONES;

    // fetch
    char *buf = kv_ns_get_str_atomic(kv_get_ns_zone(), zone->id); // result MUST BE FREED by caller
    if (buf == NULL)
    {
        ESP_LOGV(TAG, "Could not get stored mode-string");
        return false;
    }

    // parse
    int result = true;
    ESP_LOGV(TAG, "Zone config string: %s", buf);
    struct re_result *res = re_match(str_zone_config_mode_value_regex, buf);
    if (res == NULL || res->count != 4)
    {
        ESP_LOGW(TAG, "Invalid mode-string %s for zone %s", buf, zone->id);
        result = false;
    }

    enum ofp_zone_mode mode = atoi(res->strings[1]);
    int mode_value = atoi(res->strings[2]);
    char *desc = res->strings[3];

    if (!ofp_zone_set_mode(zone, mode, mode_value))
    {
        ESP_LOGW(TAG, "Could not set mode %i value %i for zone %s", mode, mode_value, zone->id);
        result = false;
    }

    if (!ofp_zone_set_description(zone, desc))
    {
        ESP_LOGW(TAG, "Could not set description zone %s : %s", zone->id, desc);
        result = false;
    }

    // cleanup
    if (res != NULL)
        re_free(res);
    free(buf);

    return result;
}

bool ofp_zone_store(struct ofp_zone *zone)
{
    assert(zone != NULL);
    ESP_LOGD(TAG, "ofp_zone_store zone id %s", zone->id);

    // mmmmmmmmm:vvvvvvvvv:ddddddddddddd....dd\0
    int len = strlen(zone->description) + 2 /* : */ + 2 * 9 /* int32 */ + 1 /* \0 */;
    char *buf = malloc(len);
    if (buf == NULL)
    {
        ESP_LOGE(TAG, "Memory allocation failed while storing zone %i", zone->mode);
        return false;
    }

    switch (zone->mode)
    {
    case HW_OFP_ZONE_MODE_FIXED:
        snprintf(buf, len, str_zone_config_mode_value_printf, zone->mode, zone->mode_data.order_id, zone->description);
        break;
    case HW_OFP_ZONE_MODE_PLANNING:
        snprintf(buf, len, str_zone_config_mode_value_printf, zone->mode, zone->mode_data.planning_id, zone->description);
        break;
    default:
        ESP_LOGE(TAG, "Unknown zone mode: %i", zone->mode);
        free(buf);
        return false;
    }

    kv_ns_set_str_atomic(kv_get_ns_zone(), zone->id, buf);

    free(buf);
    return true;
}

static void ofp_zone_check_deleted_planning(int planning_id)
{
    ESP_LOGD(TAG, "ofp_zone_check_deleted_planning planning %i", planning_id);

    struct ofp_hw *current_hw = ofp_hw_get_current();
    if (current_hw == NULL)
        return;

    // reset every zone which referenced this planning
    for (int i = 0; i < current_hw->zone_set.count; i++)
    {
        struct ofp_zone *zone = &current_hw->zone_set.zones[i];
        ESP_LOGV(TAG, "zone %i mode %i data %i", i, zone->mode, zone->mode_data.planning_id);
        if (zone->mode == HW_OFP_ZONE_MODE_PLANNING && zone->mode_data.planning_id == planning_id)
        {
            ESP_LOGI(TAG, "Setting zone %s to fixed order %i because configured planning %i was deleted, ", zone->id, DEFAULT_FIXED_ORDER_FOR_ZONES, planning_id);
            ofp_zone_set_mode_fixed(zone, DEFAULT_FIXED_ORDER_FOR_ZONES);
        }
    }
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

    // alloc and zero members
    struct ofp_zone *buf = calloc(zone_count, sizeof(struct ofp_zone));
    assert(buf != NULL);
    zone_set->count = zone_count;
    zone_set->zones = buf;

    return true;
}

/* initialize the hardware based on stored hardware id */
void ofp_hw_initialize(void)
{
    // esp_log_level_set(TAG, ESP_LOG_VERBOSE); // DEBUG
    ESP_LOGD(TAG, "ofp_hw_initialize");

    // get selected hardware id from storage
    struct ofp_hw *current_hw = ofp_get_hardware_from_stored_id();
    ESP_LOGV(TAG, "stored hardware %p", current_hw);
    if (current_hw == NULL)
    {
        ESP_LOGW(TAG, "No hardware selected, disabling hardware");
        return;
    }

    ESP_LOGI(TAG, "Initializing hardware %s", current_hw->id);

    if (!kv_set_ns_hardware(current_hw->id))
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

    if (!kv_set_ns_zone_for_hardware(current_hw->id))
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
            ESP_LOGI(TAG, "No configuration stored for zone %s, reverting to default", zone->id);
            continue;
        }
    }

    // update global variable
    hw_global = current_hw;
}

static bool ofp_zone_update_from_planning(struct ofp_zone *zone, struct tm *timeinfo)
{
    // esp_log_level_set(TAG, ESP_LOG_VERBOSE); // DEBUG

    ESP_LOGD(TAG, "ofp_zone_update_current");

    // planning exists
    struct ofp_planning *plan = ofp_planning_list_find_planning_by_id(zone->mode_data.planning_id);
    if (plan == NULL)
    {
        ESP_LOGW(TAG, "Unknown planning %i for zone %s", zone->mode_data.planning_id, zone->id);
        return false;
    }

    // find most recent slot
    int min_delta = 7 * 24 * 60 * 60; // 7 days
    int current_since_start_of_week =
        timeinfo->tm_wday * 24 * 60 * 60 +
        timeinfo->tm_hour * 60 * 60 +
        timeinfo->tm_min * 60 +
        timeinfo->tm_sec;
    ESP_LOGV(TAG, "current since start of week: %i", current_since_start_of_week);
    struct ofp_planning_slot *most_recent_slot = NULL;
    for (int i = 0; i < OFP_MAX_PLANNING_SLOT_COUNT; i++)
    {
        struct ofp_planning_slot *slot = plan->slots[i];

        if (slot == NULL)
            continue;

        ESP_LOGV(TAG, "planning %i slot %ih%i", plan->id, slot->hour, slot->minute);

        int slot_since_start_of_week =
            slot->dow * 24 * 60 * 60 +
            slot->hour * 60 * 60 +
            slot->minute * 60;
        ESP_LOGV(TAG, "slot since start of week: %i", slot_since_start_of_week);

        int delta = current_since_start_of_week - slot_since_start_of_week;
        ESP_LOGV(TAG, "delta %i min_delta is %i", delta, min_delta);

        // reject future slots and slots older than current
        if (delta < 0 || delta > min_delta)
        {
            ESP_LOGV(TAG, "Skipping slot");
            continue;
        }

        // keep
        ESP_LOGV(TAG, "Retaining as better");
        min_delta = delta;
        most_recent_slot = slot;
    }

    // before the first slot, or no slot at all
    if (most_recent_slot == NULL)
    {
        // TODO: rollover from order at end of the week, if any
        ESP_LOGW(TAG, "No slot found in planning %i for zone %s", zone->mode_data.planning_id, zone->id);
        return false;
    }

    // apply slot order
    zone->current = most_recent_slot->order_id;
    ESP_LOGV(TAG, "zone %s planning %i order %i", zone->id, plan->id, zone->current);
    return true;
}

void ofp_zone_update_current_orders(struct ofp_hw *hw, struct tm *timeinfo)
{
    ESP_LOGD(TAG, "ofp_zone_update_current_orders");

    enum ofp_order_id override_order_id;
    bool override_active = ofp_override_get_order_id(&override_order_id);

    // compute current zone orders
    for (int i = 0; i < hw->zone_set.count; i++)
    {
        struct ofp_zone *zone = &hw->zone_set.zones[i];

        // if there is an active override
        if (override_active)
        {
            zone->current = override_order_id;
            ESP_LOGV(TAG, "overriding zone %s with order %i", zone->id, zone->current);
            continue;
        }

        switch (zone->mode)
        {
        case HW_OFP_ZONE_MODE_FIXED:
            // if the zone has a fixed configuration
            zone->current = zone->mode_data.order_id;
            ESP_LOGV(TAG, "fixed zone %s with order %i", zone->id, zone->current);
            break;

        case HW_OFP_ZONE_MODE_PLANNING:
            // if the zone is driven by a planning
            if (!ofp_zone_update_from_planning(zone, timeinfo))
            {
                zone->current = DEFAULT_FIXED_ORDER_FOR_ZONES;
                ESP_LOGW(TAG, "Could not update zone %s current order from planning", zone->id);
            }
            break;

        default:
            zone->current = DEFAULT_FIXED_ORDER_FOR_ZONES;
            ESP_LOGW(TAG, "Unknown mode %i for zone %s", zone->mode, zone->id);
            break;
        }

        ESP_LOGV(TAG, "zone %s current %i", zone->id, zone->current);
    }
}

/* access the global hardware instance */
struct ofp_hw *ofp_hw_get_current(void)
{
    return hw_global;
}

/* planning functions */

struct ofp_planning_list *ofp_planning_list_get(void)
{
    return plan_list_global;
}

void ofp_planning_list_init(void)
{
    // esp_log_level_set(TAG, ESP_LOG_VERBOSE); // DEBUG

    ESP_LOGD(TAG, "ofp_planning_list_init");

    // init only if not yet initialized
    assert(plan_list_global == NULL);

    // alloc and zero members
    plan_list_global = calloc(1, sizeof(struct ofp_planning_list));
    assert(plan_list_global != NULL);

    // no planning yet
    plan_list_global->max_id = -1;

    // load plannings
    ESP_LOGI(TAG, "Load planning definitions");
    ofp_planning_list_load_plannings();
}

static int ofp_planning_list_get_next_planning_id(void)
{
    return ++plan_list_global->max_id;
}

struct ofp_planning *ofp_planning_list_find_planning_by_id(int planning_id)
{
    assert(plan_list_global);
    ESP_LOGD(TAG, "ofp_planning_list_find_planning_by_id %i", planning_id);

    for (int i = 0; i < OFP_MAX_PLANNING_COUNT; i++)
    {
        struct ofp_planning *plan = plan_list_global->plannings[i];
        if (plan == NULL)
            continue;
        if (plan->id == planning_id)
        {
            ESP_LOGV(TAG, "found at %i plan %p", i, plan);
            return plan;
        }
    }
    return NULL;
}

static int ofp_planning_get_next_slot_id(struct ofp_planning *plan)
{
    return ++plan->max_slot_id;
}

static struct ofp_planning *ofp_planning_init(int planning_id, char *description)
{
    assert(planning_id >= 0);
    assert(description != NULL);
    ESP_LOGD(TAG, "ofp_planning_create id %i desc %s", planning_id, description);

    // alloc and zero members
    struct ofp_planning *plan = calloc(1, sizeof(struct ofp_planning));
    if (plan == NULL)
    {
        ESP_LOGD(TAG, "calloc failed");
        return NULL;
    }

    // init main
    plan->id = planning_id;
    plan->description = strdup(description);
    if (plan->description == NULL)
    {
        ESP_LOGD(TAG, "strdup failed");
        return NULL;
    }
    return plan;
}

static void ofp_planning_free(struct ofp_planning *plan)
{
    assert(plan != NULL);
    ESP_LOGD(TAG, "ofp_planning_free planning_id %i", plan->id);

    if (plan->description)
        free(plan->description);
    free(plan);
}

static void ofp_planning_store(struct ofp_planning *plan)
{
    assert(plan != NULL);
    assert(plan->id >= 0);
    ESP_LOGD(TAG, "ofp_planning_store planning_id %i", plan->id);

    char buf[OFP_MAX_LEN_INT32];
    snprintf(buf, sizeof(buf), "%i", plan->id);
    kv_ns_set_str_atomic(kv_get_ns_plan(), buf, plan->description);
}

static void ofp_planning_purge(struct ofp_planning *plan)
{
    assert(plan != NULL);
    assert(plan->id >= 0);
    ESP_LOGD(TAG, "ofp_planning_purge planning_id %i", plan->id);

    char buf[OFP_MAX_LEN_INT32];
    snprintf(buf, sizeof(buf), "%i", plan->id);

    // clear every slots in planning ID namespace
    if (!kv_set_ns_slots_for_planning(plan->id))
        return;

    kv_ns_clear_atomic(kv_get_ns_slots());

    // delete planning in plannings namespace
    kv_ns_delete_atomic(kv_get_ns_plan(), buf);
}

static struct ofp_planning_slot *ofp_planning_slot_init(int id, enum ofp_day_of_week dow, int hour, int minute, enum ofp_order_id order_id)
{
    ESP_LOGD(TAG, "ofp_planning_slot_init id %i dow %i hour %i minute %i order_id %i", id, dow, hour, minute, order_id);

    assert(id >= 0);
    assert(ofp_day_of_week_is_valid(dow));
    assert(hour >= 0 && hour < 24);
    assert(minute >= 0 && minute < 60);
    assert(ofp_order_id_is_valid(order_id));

    // alloc and zero members
    struct ofp_planning_slot *slot = calloc(1, sizeof(struct ofp_planning_slot));
    assert(slot != NULL);

    // init
    slot->id = id;
    slot->dow = dow;
    slot->hour = hour;
    slot->minute = minute;
    slot->order_id = order_id;

    ESP_LOGV(TAG, "slot id %i p %p", slot->id, slot);
    return slot;
}

static void ofp_planning_slot_free(struct ofp_planning_slot *slot)
{
    assert(slot != NULL);
    ESP_LOGD(TAG, "ofp_planning_slot_free slot id %i p %p", slot->id, slot);
    free(slot);
}

static void ofp_planning_slot_store(int planning_id, struct ofp_planning_slot *slot)
{
    assert(slot != NULL);
    ESP_LOGD(TAG, "ofp_planning_slot_store planning_id %i slot_id %i", planning_id, slot->id);

    if (!kv_set_ns_slots_for_planning(planning_id))
        return;

    char key[OFP_MAX_LEN_INT32];
    snprintf(key, sizeof(key), "%i", slot->id);

    char val[OFP_MAX_LEN_PLANNING_SLOT_VALUE];
    snprintf(val, sizeof(val), str_planning_slot_value_printf, slot->dow, slot->hour, slot->minute, slot->order_id);

    kv_ns_set_str_atomic(kv_get_ns_slots(), key, val);
}

static void ofp_planning_slot_purge(int planning_id, struct ofp_planning_slot *slot)
{
    assert(slot != NULL);
    ESP_LOGD(TAG, "ofp_planning_slot_purge planning_id %i slot_id %i", planning_id, slot->id);

    if (!kv_set_ns_slots_for_planning(planning_id))
        return;

    char key[OFP_MAX_LEN_INT32];
    snprintf(key, sizeof(key), "%i", slot->id);
    kv_ns_delete_atomic(kv_get_ns_slots(), key);
}

static bool ofp_planning_add_slot(struct ofp_planning *planning, struct ofp_planning_slot *slot)
{
    assert(planning != NULL);
    assert(slot != NULL);

    ESP_LOGD(TAG, "ofp_planning_add_slot planning %i slot %i", planning->id, slot->id);

    // keep max_slot_id in sync if needed
    if (slot->id > planning->max_slot_id)
    {
        planning->max_slot_id = slot->id;
        ESP_LOGV(TAG, "max_slot_id updated to %i", planning->max_slot_id);
    }

    for (int i = 0; i < OFP_MAX_PLANNING_SLOT_COUNT; i++)
    {
        struct ofp_planning_slot **candidate = &planning->slots[i];
        ESP_LOGV(TAG, "index %i %p", i, *candidate);
        if (*candidate == NULL)
        {
            *candidate = slot;
            ESP_LOGV(TAG, "stored %p", *candidate);
            return true;
        }
    }
    return false;
}

static struct ofp_planning_slot *ofp_planning_slot_find_by_id(struct ofp_planning *plan, int slot_id)
{
    assert(plan != NULL);
    ESP_LOGD(TAG, "ofp_planning_slot_find_by_id planning_id %i slot_id %i", plan->id, slot_id);

    for (int i = 0; i < OFP_MAX_PLANNING_SLOT_COUNT; i++)
    {
        struct ofp_planning_slot *slot = plan->slots[i];
        if (slot == NULL)
            continue;
        if (slot->id == slot_id)
        {
            ESP_LOGV(TAG, "found at %i slot %p", i, slot);
            return slot;
        }
    }
    return NULL;
}

static struct ofp_planning_slot *ofp_planning_slot_find_by_values(struct ofp_planning *plan, enum ofp_day_of_week dow, int hour, int minute)
{
    assert(plan != NULL);
    ESP_LOGD(TAG, "ofp_planning_slot_find_by_values planning_id %i dow %i hour %i minute %i", plan->id, dow, hour, minute);

    for (int i = 0; i < OFP_MAX_PLANNING_SLOT_COUNT; i++)
    {
        struct ofp_planning_slot *s = plan->slots[i];
        ESP_LOGV(TAG, "index %i %p", i, s);
        if (s == NULL)
            continue;
        if (s->dow == dow && s->hour == hour && s->minute == minute)
        {
            ESP_LOGV(TAG, "found at %i slot %p", i, s);
            return s;
        }
    }
    return NULL;
}

bool ofp_planning_add_new_slot(int planning_id, enum ofp_day_of_week dow, int hour, int minute, enum ofp_order_id order_id)
{
    ESP_LOGD(TAG, "ofp_planning_add_new_slot planning_id %i dow %i hour %i minute %i order_id %i", planning_id, dow, hour, minute, order_id);

    struct ofp_planning *plan = ofp_planning_list_find_planning_by_id(planning_id);
    if (plan == NULL)
    {
        ESP_LOGW(TAG, "Could not find planning %i", planning_id);
        return false;
    }

    // duplicates
    struct ofp_planning_slot *slot = ofp_planning_slot_find_by_values(plan, dow, hour, minute);
    if (slot != NULL)
    {
        ESP_LOGW(TAG, "Planning slot %i already exists in planning %i with dow %i hour %i minute %i ", slot->id, plan->id, slot->dow, slot->hour, slot->minute);
        return false;
    }

    // create
    int slot_id = ofp_planning_get_next_slot_id(plan);
    slot = ofp_planning_slot_init(slot_id, dow, hour, minute, order_id);
    if (plan == NULL)
    {
        ESP_LOGW(TAG, "Could not create new planning slot");
        return false;
    }

    ofp_planning_slot_store(planning_id, slot);

    if (!ofp_planning_add_slot(plan, slot))
    {
        ESP_LOGW(TAG, "Could not add slot %ih%i to planning %i, skipping slot", slot->hour, slot->minute, plan->id);
        ofp_planning_slot_free(slot);
        return false;
    }

    return true;
}

static bool ofp_planning_remove_slot(struct ofp_planning *plan, int slot_id)
{
    assert(plan != NULL);
    ESP_LOGD(TAG, "ofp_planning_remove_slot planning %i slot %i", plan->id, slot_id);

    // search and prune
    struct ofp_planning_slot *slot = NULL;
    for (int i = 0; i < OFP_MAX_PLANNING_SLOT_COUNT; i++)
    {
        struct ofp_planning_slot **candidate = &plan->slots[i];
        ESP_LOGV(TAG, "index %i %p", i, *candidate);
        if (*candidate == NULL)
            continue;

        if ((*candidate)->id != slot_id)
            continue;

        if ((*candidate)->dow == OFP_DOW_SUNDAY /* 0 */ && (*candidate)->hour == 0 && (*candidate)->minute == 0)
        {
            ESP_LOGW(TAG, "Could not remove first slot slot for planning %i", plan->id);
            return false;
        }

        slot = *candidate;
        *candidate = NULL; // remove from slot list
        ESP_LOGV(TAG, "found and removed %p", slot);
        break;
    }

    if (slot == NULL)
    {
        ESP_LOGD(TAG, "slot %i not found", slot_id);
        return false;
    }

    ofp_planning_slot_purge(plan->id, slot);

    ofp_planning_slot_free(slot);

    return true;
}

bool ofp_planning_remove_existing_slot(int planning_id, int slot_id)
{
    ESP_LOGD(TAG, "ofp_planning_remove_slot planning %i slot %i", planning_id, slot_id);

    struct ofp_planning *plan = ofp_planning_list_find_planning_by_id(planning_id);
    if (plan == NULL)
    {
        ESP_LOGW(TAG, "Could not find planning %i", planning_id);
        return false;
    }

    return ofp_planning_remove_slot(plan, slot_id);
}

/*
 * IMPORTANT:
 *
 * As stored plannings come with their already set IDs,
 * you MUST load stored plannings BEFORE creating new plannings,
 *
 * If you do not respect this, you could have duplicate planning IDs
 */
static bool ofp_planning_list_add_planning(struct ofp_planning *planning)
{
    assert(planning != NULL);
    assert(plan_list_global != NULL);

    ESP_LOGD(TAG, "ofp_planning_list_add_planning planning %p %i", planning, planning->id);

    // keep max_id in sync if needed
    if (planning->id > plan_list_global->max_id)
    {
        plan_list_global->max_id = planning->id;
        ESP_LOGV(TAG, "max_id updated to %i", plan_list_global->max_id);
    }

    // add to the list
    for (int i = 0; i < OFP_MAX_PLANNING_COUNT; i++)
    {
        struct ofp_planning **candidate = &plan_list_global->plannings[i];
        ESP_LOGV(TAG, "index %i %p", i, *candidate);

        if (*candidate != NULL)
            continue;

        *candidate = planning;
        ESP_LOGV(TAG, "stored %p", *candidate);

        return true;
    }

    return false; // No more space available
}

static void ofp_planning_load_slots(struct ofp_planning *plan)
{
    assert(plan != NULL);
    ESP_LOGD(TAG, "ofp_planning_load_slots planning_id %i", plan->id);

    // search slots
    if (!kv_set_ns_slots_for_planning(plan->id))
        return;
    nvs_iterator_t it_slot = nvs_entry_find(default_nvs_partition_name, kv_get_ns_slots(), NVS_TYPE_STR);
    for (; it_slot != NULL; it_slot = nvs_entry_next(it_slot))
    {
        nvs_entry_info_t info;
        nvs_entry_info(it_slot, &info);

        int slot_id;
        if (!parse_int(info.key, &slot_id))
        {
            ESP_LOGW(TAG, "Ignoring slot with invalid key for planning %i: %s", plan->id, info.key);
            return;
        }

        // check for duplicates (based on IDs, not inner data)
        if (ofp_planning_slot_find_by_id(plan, slot_id) != NULL)
        {
            ESP_LOGW(TAG, "Ignoring duplicate slot id found in planning %i namespace: %i", plan->id, slot_id);
            continue;
        }

        // get
        char *value = kv_ns_get_str_atomic(kv_get_ns_slots(), info.key); // must be free'd
        if (value == NULL)
        {
            ESP_LOGW(TAG, "Could not read slot id %i found in planning %i namespace, ignoring", slot_id, plan->id);
            continue;
        }

        // parse
        struct re_result *res = re_match(str_planning_slot_value_regex, value);
        if (res == NULL || res->count != 5)
        {
            ESP_LOGW(TAG, "Ignoring invalid slot configuration %s for slot %i of planning %i", value, slot_id, plan->id);
            continue;
        }
        enum ofp_day_of_week dow = atoi(res->strings[1]);
        int hour = atoi(res->strings[2]);
        int minute = atoi(res->strings[3]);
        enum ofp_order_id order_id = atoi(res->strings[4]);

        // checking
        if (!ofp_day_of_week_is_valid(dow))
        {
            ESP_LOGW(TAG, "Ignoring planning %i slot %i with invalid day of week %i", plan->id, slot_id, dow);
            continue;
        }
        if (hour < 0 || hour >= 24)
        {
            ESP_LOGW(TAG, "Ignoring planning %i slot %i with invalid hour %i", plan->id, slot_id, hour);
            continue;
        }
        if (minute < 0 || minute >= 60)
        {
            ESP_LOGW(TAG, "Ignoring planning %i slot %i with invalid hour %i", plan->id, slot_id, minute);
            continue;
        }
        if (!ofp_order_id_is_valid(order_id))
        {
            ESP_LOGW(TAG, "Ignoring planning %i slot %i with invalid order id %i", plan->id, slot_id, order_id);
            continue;
        }

        // create
        struct ofp_planning_slot *slot = ofp_planning_slot_init(slot_id, dow, hour, minute, order_id);
        if (slot == NULL)
        {
            ESP_LOGW(TAG, "Could not create slot %s for planning %i, skipping slot", info.key, plan->id);
            continue;
        }

        // add
        if (!ofp_planning_add_slot(plan, slot))
        {
            ESP_LOGW(TAG, "Could not add slot %ih%i to planning %i, skipping slot", slot->hour, slot->minute, plan->id);
            ofp_planning_slot_free(slot);
            continue;
        }
    }
    nvs_release_iterator(it_slot);
}

static void ofp_planning_list_load_plannings(void)
{
    ESP_LOGD(TAG, "ofp_planning_list_load_plannings");

    // search plannings
    nvs_iterator_t it_plan = nvs_entry_find(default_nvs_partition_name, kv_get_ns_plan(), NVS_TYPE_STR);
    for (; it_plan != NULL; it_plan = nvs_entry_next(it_plan))
    {
        nvs_entry_info_t info;
        nvs_entry_info(it_plan, &info);

        // check key (which is planning_id)
        int planning_id;
        if (!parse_int(info.key, &planning_id) || planning_id < 0)
        {
            ESP_LOGW(TAG, "Ignoring invalid key found in planning namespace: %s", info.key);
            continue;
        }

        // check for duplicates
        if (ofp_planning_list_find_planning_by_id(planning_id))
        {
            ESP_LOGW(TAG, "Ignoring duplicate planning id found in planning namespace: %s", info.key);
            continue;
        }

        // create planning
        ESP_LOGV(TAG, "Found planning_id: %i", planning_id);
        char *description = kv_ns_get_str_atomic(kv_get_ns_plan(), info.key);
        if (description == NULL)
        {
            ESP_LOGW(TAG, "Could not read description for planning %i, skipping planning", planning_id);
            continue;
        }

        struct ofp_planning *plan = ofp_planning_init(planning_id, description);
        if (plan == NULL)
        {
            ESP_LOGW(TAG, "Could not create planning %i, skipping planning", planning_id);
            continue;
        }

        // add to list
        if (!ofp_planning_list_add_planning(plan))
        {
            ESP_LOGW(TAG, "Could not add planning %i, skipping planning", plan->id);
            ofp_planning_free(plan);
            continue;
        }

        // load slots
        ofp_planning_load_slots(plan);
    }
    nvs_release_iterator(it_plan);
}

static struct ofp_planning *ofp_planning_list_find_planning_by_description(const char *description)
{
    assert(description != NULL);
    ESP_LOGD(TAG, "ofp_planning_list_find_planning_by_description desc %s", description);

    for (int i = 0; i < OFP_MAX_PLANNING_COUNT; i++)
    {
        struct ofp_planning *plan = plan_list_global->plannings[i];
        ESP_LOGV(TAG, "index %i %p", i, plan);

        if (plan == NULL)
            continue;

        if (strcmp(description, plan->description) == 0)
            return plan;
    }
    return NULL;
}

bool ofp_planning_list_add_new_planning(char *description)
{
    assert(description != NULL);
    ESP_LOGD(TAG, "ofp_planning_list_add_new_planning desc %s", description);

    // check for duplicate description
    if (ofp_planning_list_find_planning_by_description(description))
    {
        ESP_LOGW(TAG, "Planning with same description already exists");
        return false;
    }

    // initialize
    struct ofp_planning *plan = ofp_planning_init(ofp_planning_list_get_next_planning_id(), description);
    if (plan == NULL)
    {
        ESP_LOGW(TAG, "Could not initialize new planning");
        return false;
    }

    ofp_planning_store(plan);

    if (!ofp_planning_list_add_planning(plan))
    {
        ESP_LOGW(TAG, "Could not add planning %i, skipping planning", plan->id);
        ofp_planning_free(plan);
        return false;
    }

    int slot_id = ofp_planning_get_next_slot_id(plan);
    struct ofp_planning_slot *slot = ofp_planning_slot_init(slot_id, OFP_DOW_SUNDAY, 0, 0, DEFAULT_FIXED_ORDER_FOR_ZONES); // TODO: ID
    if (slot == NULL)
    {
        ESP_LOGW(TAG, "Could not initialize new slot for planning %i", plan->id);
        return false;
    }

    ofp_planning_slot_store(plan->id, slot);

    if (!ofp_planning_add_slot(plan, slot))
    {
        ESP_LOGW(TAG, "Could not add slot %ih%i to planning %i, skipping slot", slot->hour, slot->minute, plan->id);
        ofp_planning_slot_free(slot);
        return false;
    }

    return true;
}

bool ofp_planning_list_remove_planning(int planning_id)
{
    ESP_LOGD(TAG, "ofp_planning_list_remove_planning planning_id %i", planning_id);

    // search and prune
    struct ofp_planning *plan = NULL;
    for (int i = 0; i < OFP_MAX_PLANNING_COUNT; i++)
    {
        struct ofp_planning **candidate = &plan_list_global->plannings[i];
        ESP_LOGV(TAG, "index %i %p", i, *candidate);

        if (*candidate == NULL)
            continue;

        if ((*candidate)->id != planning_id)
            continue;

        plan = *candidate;
        *candidate = NULL; // remove from planning list
        ESP_LOGV(TAG, "found %p and removed from list", plan);
        break;
    }

    if (plan == NULL)
    {
        ESP_LOGD(TAG, "planning %i not found", planning_id);
        return false;
    }

    // remove slots (including the default slot)
    for (int i = 0; i < OFP_MAX_PLANNING_SLOT_COUNT; i++)
    {
        struct ofp_planning_slot **candidate = &plan->slots[i];
        if (*candidate == NULL)
            continue;
        ofp_planning_slot_purge(plan->id, *candidate);
        ofp_planning_slot_free(*candidate);
        *candidate = NULL; // remove from slot list
    }

    // de-reference planning from zones
    ofp_zone_check_deleted_planning(plan->id);

    ofp_planning_purge(plan);

    ofp_planning_free(plan);

    return plan;
}

bool ofp_planning_change_description(int planning_id, char *description)
{
    assert(description != NULL);
    ESP_LOGD(TAG, "ofp_planning_change_description planning_id %i description %s", planning_id, description);

    // check for duplicate description
    if (ofp_planning_list_find_planning_by_description(description))
    {
        ESP_LOGW(TAG, "Planning with same description already exists");
        return false;
    }

    struct ofp_planning *plan = ofp_planning_list_find_planning_by_id(planning_id);
    if (plan == NULL)
    {
        ESP_LOGW(TAG, "Planning %i not found", planning_id);
        return false;
    }

    if (plan->description != NULL)
        free(plan->description);
    plan->description = strdup(description);
    ESP_LOGV(TAG, "Description changed in memory");

    ofp_planning_store(plan);

    return true;
}

bool ofp_planning_slot_set_order(int planning_id, int slot_id, enum ofp_order_id order_id)
{
    ESP_LOGD(TAG, "ofp_planning_slot_set_order planning_id %i slot_id %i order_id %i", planning_id, slot_id, order_id);

    assert(ofp_order_id_is_valid(order_id));

    struct ofp_planning *plan = ofp_planning_list_find_planning_by_id(planning_id);
    if (plan == NULL)
    {
        ESP_LOGW(TAG, "Could not find planning %i", planning_id);
        return false;
    }

    struct ofp_planning_slot *slot = ofp_planning_slot_find_by_id(plan, slot_id);
    if (slot == NULL)
    {
        ESP_LOGW(TAG, "Could not find slot %i in planning %i", slot_id, plan->id);
        return false;
    }

    if (slot->order_id == order_id)
    {
        ESP_LOGV(TAG, "order_id not changed");
        return true;
    }

    ESP_LOGV(TAG, "Old slot order_id %i", slot->order_id);
    slot->order_id = order_id;
    ESP_LOGV(TAG, "New slot order_id %i", slot->order_id);
    ofp_planning_slot_store(plan->id, slot);

    return true;
}

bool ofp_planning_slot_set_dow(int planning_id, int slot_id, int dow)
{
    ESP_LOGD(TAG, "ofp_planning_slot_set_dow planning_id %i slot_id %i dow %i", planning_id, slot_id, dow);

    assert(ofp_day_of_week_is_valid(dow));

    struct ofp_planning *plan = ofp_planning_list_find_planning_by_id(planning_id);
    if (plan == NULL)
    {
        ESP_LOGW(TAG, "Could not find planning %i", planning_id);
        return false;
    }

    struct ofp_planning_slot *slot = ofp_planning_slot_find_by_id(plan, slot_id);
    if (slot == NULL)
    {
        ESP_LOGW(TAG, "Could not find slot %i in planning %i", slot_id, plan->id);
        return false;
    }

    if (slot->dow == dow)
    {
        ESP_LOGV(TAG, "dow not changed");
        return true;
    }

    ESP_LOGV(TAG, "Old slot dow %i", slot->dow);
    slot->dow = dow;
    ESP_LOGV(TAG, "New slot dow %i", slot->dow);
    ofp_planning_slot_store(plan->id, slot);

    return true;
}

bool ofp_planning_slot_set_hour(int planning_id, int slot_id, int hour)
{
    ESP_LOGD(TAG, "ofp_planning_slot_set_hour planning_id %i slot_id %i hour %i", planning_id, slot_id, hour);

    assert(hour >= 0 && hour < 24);

    struct ofp_planning *plan = ofp_planning_list_find_planning_by_id(planning_id);
    if (plan == NULL)
    {
        ESP_LOGW(TAG, "Could not find planning %i", planning_id);
        return false;
    }

    struct ofp_planning_slot *slot = ofp_planning_slot_find_by_id(plan, slot_id);
    if (slot == NULL)
    {
        ESP_LOGW(TAG, "Could not find slot %i in planning %i", slot_id, plan->id);
        return false;
    }

    if (slot->hour == hour)
    {
        ESP_LOGV(TAG, "hour not changed");
        return true;
    }

    ESP_LOGV(TAG, "Old slot hour %i", slot->hour);
    slot->hour = hour;
    ESP_LOGV(TAG, "New slot hour %i", slot->hour);
    ofp_planning_slot_store(plan->id, slot);

    return true;
}

bool ofp_planning_slot_set_minute(int planning_id, int slot_id, int minute)
{
    ESP_LOGD(TAG, "ofp_planning_slot_set_minute planning_id %i slot_id %i minute %i", planning_id, slot_id, minute);

    assert(minute >= 0 && minute < 60);

    struct ofp_planning *plan = ofp_planning_list_find_planning_by_id(planning_id);
    if (plan == NULL)
    {
        ESP_LOGW(TAG, "Could not find planning %i", planning_id);
        return false;
    }

    struct ofp_planning_slot *slot = ofp_planning_slot_find_by_id(plan, slot_id);
    if (slot == NULL)
    {
        ESP_LOGW(TAG, "Could not find slot %i in planning %i", slot_id, plan->id);
        return false;
    }

    if (slot->minute == minute)
    {
        ESP_LOGV(TAG, "minute not changed");
        return true;
    }

    ESP_LOGV(TAG, "Old slot minute %i", slot->minute);
    slot->minute = minute;
    ESP_LOGV(TAG, "New slot minute %i", slot->minute);
    ofp_planning_slot_store(plan->id, slot);

    return true;
}
