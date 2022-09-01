#include <string.h>

#include <esp_log.h>

#include "str.h"
#include "ofp.h"
#include "storage.h"
#include "api_hw.h"

static const char TAG[] = "ofp";

/* defines */
#define DEFAULT_FIXED_ORDER_FOR_ZONES HW_OFP_ORDER_ID_STANDARD_COZY
#define OFP_MAX_LEN_INT32 11

/* constants */
static const char str_re_zone_config_mode_value[] = "^m([[:digit:]]+):v([[:digit:]]+)^";
static const char str_planning_slot_id_start_printf[] = "%02ih%02i";

/* global hardware instance, get it using ofp_hw_get() */
static struct ofp_hw *hw_global = NULL;

/* global planning instance, get it using ofp_planning_list_get() */
struct ofp_planning_list *plan_list_global = NULL;

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

/* private forward declarations */
static struct ofp_planning *ofp_planning_list_find_planning_by_id(int planning_id);

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
    if (ofp_planning_list_find_planning_by_id(planning_id) != NULL)
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
    char *buf = kv_ns_get_str_atomic(kv_get_ns_zone(), zone->id); // result MUST BE FREED by caller
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
            ESP_LOGW(TAG, "Could not load configuration for zone %s, reverting to default", zone->id);
            continue;
        }
    }

    // update global variable
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

    char buf[LOCALTIME_TO_STRING_BUFFER_LENGTH];
    localtime_to_string(&ti, buf, sizeof(buf));
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

/* planning functions */

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

    // TODO: load plannings
}

static int ofp_planning_list_get_next_planning_id(void)
{
    return ++plan_list_global->max_id;
}

static struct ofp_planning *ofp_planning_list_find_planning_by_id(int planning_id)
{
    ESP_LOGD(TAG, "ofp_planning_list_find_planning_by_id %i", planning_id);

    for (int i = 0; i < OFP_MAX_PLANNING_COUNT; i++)
    {
        struct ofp_planning *plan = plan_list_global->plannings[i];
        if (plan == NULL)
            continue;
        if (plan->id == planning_id)
        {
            ESP_LOGV(TAG, "found at %i", i);
            return plan;
        }
    }
    return NULL;
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
    kv_set_ns_slots_for_planning(plan->id);
    kv_ns_clear_atomic(kv_get_ns_slots());

    // delete planning in plannings namespace
    kv_ns_delete_atomic(kv_get_ns_plan(), buf);
}

static struct ofp_planning_slot *ofp_planning_slot_init(int hour, int minute, enum ofp_order_id order_id)
{
    assert(hour >= 0 && hour < 24);
    assert(minute >= 0 && minute < 60);
    assert(ofp_order_id_is_valid(order_id));

    ESP_LOGD(TAG, "ofp_planning_slot_init hour %i minute %i order_id %i", hour, minute, order_id);

    // alloc and zero members
    struct ofp_planning_slot *slot = calloc(1, sizeof(struct ofp_planning_slot));
    assert(slot != NULL);

    // init
    snprintf(slot->id_start, OFP_MAX_LEN_PLANNING_START, str_planning_slot_id_start_printf, hour, minute);
    slot->order_id = order_id;

    ESP_LOGV(TAG, "slot %s order_id %i", slot->id_start, slot->order_id);
    return slot;
}

static void ofp_planning_slot_free(struct ofp_planning_slot *slot)
{
    assert(slot != NULL);
    ESP_LOGD(TAG, "ofp_planning_slot_free slot %s", slot->id_start);
    free(slot);
}

static void ofp_planning_slot_store(int planning_id, struct ofp_planning_slot *slot)
{
    assert(slot != NULL);
    ESP_LOGD(TAG, "ofp_planning_slot_store planning_id %i slot %s", planning_id, slot->id_start);

    kv_set_ns_slots_for_planning(planning_id);
    kv_ns_set_i32_atomic(kv_get_ns_slots(), slot->id_start, slot->order_id);
}

static void ofp_planning_slot_purge(int planning_id, struct ofp_planning_slot *slot)
{
    assert(slot != NULL);
    ESP_LOGD(TAG, "ofp_planning_slot_purge planning_id %i slot %s", planning_id, slot->id_start);

    kv_set_ns_slots_for_planning(planning_id);
    kv_ns_delete_atomic(kv_get_ns_slots(), slot->id_start);
}

static bool ofp_planning_add_slot(struct ofp_planning *planning, struct ofp_planning_slot *slot)
{
    assert(planning != NULL);
    assert(slot != NULL);

    ESP_LOGD(TAG, "ofp_planning_add_slot planning %i slot %s", planning->id, slot->id_start);

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

static struct ofp_planning_slot *ofp_planning_slot_find_by_id(struct ofp_planning *plan, int hour, int minute)
{
    assert(plan != NULL);
    ESP_LOGD(TAG, "ofp_planning_slot_find_by_id planning_id %i hour %i minute %i", plan->id, hour, minute);

    char buf[OFP_MAX_LEN_PLANNING_START];
    snprintf(buf, sizeof(buf), str_planning_slot_id_start_printf, hour, minute);

    for (int i = 0; i < OFP_MAX_PLANNING_SLOT_COUNT; i++)
    {
        struct ofp_planning_slot *slot = plan->slots[i];
        ESP_LOGV(TAG, "index %i %p", i, slot);
        if (slot == NULL)
            continue;
        if (strcmp(slot->id_start, buf) == 0)
            return slot;
    }
    return NULL;
}

bool ofp_planning_add_new_slot(int planning_id, int hour, int minute, enum ofp_order_id order_id)
{
    ESP_LOGD(TAG, "ofp_planning_add_new_slot planning_id %i hour %i minute %i order_id %i", planning_id, hour, minute, order_id);

    struct ofp_planning *plan = ofp_planning_list_find_planning_by_id(planning_id);
    if (plan == NULL)
    {
        ESP_LOGD(TAG, "Could not find planning %i", planning_id);
        return false;
    }

    struct ofp_planning_slot *slot = ofp_planning_slot_find_by_id(plan, hour, minute);
    if (slot != NULL)
    {
        ESP_LOGW(TAG, "Planning slot %02ih%02i already exists for planning %i", hour, minute, plan->id);
        return false;
    }

    slot = ofp_planning_slot_init(hour, minute, order_id);
    if (plan == NULL)
    {
        ESP_LOGW(TAG, "Could not create new planning slot");
        return false;
    }

    ofp_planning_slot_store(planning_id, slot);

    ofp_planning_add_slot(plan, slot);

    return true;
}

static bool ofp_planning_remove_slot(struct ofp_planning *plan, int hour, int minute)
{
    assert(hour >= 0 && hour < 24);
    assert(minute >= 0 && minute < 60);
    assert(plan != NULL);
    ESP_LOGD(TAG, "ofp_planning_remove_slot planning %i hour %i minude %i", plan->id, hour, minute);

    if (hour == 0 && minute == 0)
    {
        ESP_LOGW(TAG, "Could not remove midnight slot for planning %i", plan->id);
        return false;
    }

    char buf[OFP_MAX_LEN_PLANNING_START];
    snprintf(buf, sizeof(buf), str_planning_slot_id_start_printf, hour, minute);

    // search and prune
    struct ofp_planning_slot *slot = NULL;
    for (int i = 0; i < OFP_MAX_PLANNING_SLOT_COUNT; i++)
    {
        struct ofp_planning_slot **candidate = &plan->slots[i];
        ESP_LOGV(TAG, "index %i %p", i, *candidate);
        if (*candidate == NULL)
            continue;

        if (strcmp((*candidate)->id_start, buf) != 0)
            continue;

        slot = *candidate;
        *candidate = NULL; // remove from slot list
        ESP_LOGV(TAG, "found and removed %p", slot);
        break;
    }

    if (slot == NULL)
    {
        ESP_LOGD(TAG, "slot %s not found", buf);
        return false;
    }

    ofp_planning_slot_purge(plan->id, slot);

    ofp_planning_slot_free(slot);

    return true;
}

bool ofp_planning_remove_existing_slot(int planning_id, int hour, int minute)
{
    ESP_LOGD(TAG, "ofp_planning_remove_slot planning %i hour %i minude %i", planning_id, hour, minute);

    assert(hour >= 0 && hour < 24);
    assert(minute >= 0 && minute < 60);

    struct ofp_planning *plan = ofp_planning_list_find_planning_by_id(planning_id);
    if (plan == NULL)
    {
        ESP_LOGD(TAG, "Could not find planning %i", planning_id);
        return false;
    }

    return ofp_planning_remove_slot(plan, hour, minute);
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

bool ofp_planning_list_add_new_planning(char *description)
{
    assert(description != NULL);
    ESP_LOGD(TAG, "ofp_planning_list_add_new_planning desc %s", description);

    // initialize
    struct ofp_planning *plan = ofp_planning_init(ofp_planning_list_get_next_planning_id(), description);
    if (plan == NULL)
    {
        ESP_LOGW(TAG, "Could not initialize new planning");
        return false;
    }

    ofp_planning_store(plan);

    ofp_planning_list_add_planning(plan);

    struct ofp_planning_slot *slot = ofp_planning_slot_init(0, 0, DEFAULT_FIXED_ORDER_FOR_ZONES);
    if (slot == NULL)
    {
        ESP_LOGW(TAG, "Could not initialize new slot for planning %i", plan->id);
        return false;
    }

    ofp_planning_slot_store(plan->id, slot);

    if (!ofp_planning_add_slot(plan, slot))
    {
        ESP_LOGW(TAG, "Could not add default slot for planning %i", plan->id);
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

    ofp_planning_purge(plan);

    ofp_planning_free(plan);

    return plan;
}

bool ofp_planning_change_description(int planning_id, char *description)
{
    assert(description != NULL);
    ESP_LOGD(TAG, "ofp_planning_change_description planning_id %i description %s", planning_id, description);

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

bool ofp_planning_slot_set_order(int planning_id, int hour, int minute, enum ofp_order_id order_id)
{
    ESP_LOGD(TAG, "ofp_planning_slot_set_order planning_id %i hour %i minute %i order_id %i", planning_id, hour, minute, order_id);

    assert(hour >= 0 && hour < 24);
    assert(minute >= 0 && minute < 60);
    assert(ofp_order_id_is_valid(order_id));

    struct ofp_planning *plan = ofp_planning_list_find_planning_by_id(planning_id);
    if (plan == NULL)
    {
        ESP_LOGW(TAG, "Could not find planning %i", planning_id);
        return false;
    }

    struct ofp_planning_slot *slot = ofp_planning_slot_find_by_id(plan, hour, minute);
    if (slot == NULL)
    {
        ESP_LOGW(TAG, "Could not find slot %02ih%02i in planning %i", hour, minute, plan->id);
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
