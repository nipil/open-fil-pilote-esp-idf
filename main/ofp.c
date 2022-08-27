#include <string.h>

#include <esp_log.h>

#include "str.h"
#include "ofp.h"
#include "api_hw.h"

static const char TAG[] = "ofp";

/* global hardware instance, get it using ofp_hw_get() */
static struct ofp_hw *hw_global = NULL;

static struct ofp_hw_list hw_list = {.hw_count = 0, .hw = {NULL}};

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

/* initialize the hardware based on stored hardware id */
void ofp_hw_initialize(void)
{
    ESP_LOGD(TAG, "ofp_hw_initialize");

    // stored hardware id
    struct ofp_hw *current_hw = ofp_get_hardware_from_stored_id();
    ESP_LOGD(TAG, "stored hardware %p", current_hw);
    if (current_hw == NULL)
    {
        ESP_LOGW(TAG, "No hardware currently selected, disabling hardware processing");
        return;
    }

    // initialize
    if (!current_hw->hw_func.init(current_hw))
    {
        ESP_LOGE(TAG, "Unable to initialize hardware %s, disabling hardware processing", current_hw->id);
        // return;
    }

    // persist reference
    hw_global = current_hw;
}

/* access the global hardware instance */
struct ofp_hw *ofp_hw_get_current(void)
{
    return hw_global;
}