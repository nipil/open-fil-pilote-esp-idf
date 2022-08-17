#include <string.h>

#include <esp_log.h>

#include "ofp.h"

static const char TAG[] = "ofp";

static struct ofp_hw *ofp_hardware_definitions[OFP_MAX_SIMULTANEOUS_HARDWARE] = {NULL};

/*
 * Make a hardware available to the system.
 *
 * Stores the pointer to its definition in the first available slot
 */
void ofp_hw_register(struct ofp_hw *hw)
{
    assert(hw);

    ESP_LOGD(TAG, "Registering hardware definition %s with %i parameters", hw->id, hw->param_count);

    struct ofp_hw **cur = ofp_hardware_definitions;
    struct ofp_hw **last = cur + OFP_MAX_SIMULTANEOUS_HARDWARE;
    while (cur != last)
    {
        if (*cur == NULL)
        {
            *cur = hw;
            return;
        }
        cur++;
    }

    assert(cur != last);
}