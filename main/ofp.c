#include <string.h>

#include <esp_log.h>

#include "ofp.h"

static const char TAG[] = "ofp";

static struct ofp_hw_list hw_list = {.hw_count = 0, .hw = {NULL}};

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