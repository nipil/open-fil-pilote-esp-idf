#include <stddef.h>
#include <esp_log.h>

#include "str.h"
#include "hw_m1e1.h"

static const char TAG[] = "m1e1";

/* forward definitions */
static bool hw_m1e1_zone_set_init(struct ofp_hw *hw);
static bool hw_m1e1_zone_set_apply(struct ofp_hw *hw);

/* hardware properties */
static struct ofp_hw hw_m1e1 = {
    .id = "M1E1",
    .description = "Based on a DevKit NodeMCU 30 pin and specialized mainboard M1 and expansion boards E1",
    .param_count = 1,
    .params = {
        {.id = "e1_count",
         .description = "Number of attached E1 boards",
         .type = HW_OFP_PARAM_INTEGER,
         .value = {.int_ = 0}}},
    .zone_set = {
        .count = 0,
        .zones = NULL,
    },
    .hw_hooks = {
        .init = hw_m1e1_zone_set_init,
        .apply = hw_m1e1_zone_set_apply,
    },
};

/* used for registering available hardware */
struct ofp_hw *hw_m1e1_get_definition(void)
{
    return &hw_m1e1;
}

/* init dynamic data and setup hardware */
static bool hw_m1e1_zone_set_init(struct ofp_hw *hw)
{
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);

    ESP_LOGD(TAG, "hw_m1e1_zone_set_init %p", hw);
    assert(hw != NULL);

    return false;
}

/* apply dynamic state to hardware */
static bool hw_m1e1_zone_set_apply(struct ofp_hw *hw)
{
    ESP_LOGD(TAG, "hw_m1e1_zone_set_apply %p", hw);
    assert(hw != NULL);

    return false;
}