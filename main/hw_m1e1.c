#include <stddef.h>
#include <stdio.h>
#include <esp_log.h>

#include "str.h"
#include "hw_m1e1.h"
#include "s2p_595.h"

static const char TAG[] = "m1e1";

/* defines */

// using VP pin for CFG button
#define M1_PIN_CONFIG 36

// pins for MUX
#define M1_PIN_595_IN 16
#define M1_PIN_595_SC 17
#define M1_PIN_595_LC 18
#define M1_PIN_595_RST 21
#define M1_PIN_595_OE 23

#define HW_M1E1_ZONE_ID_FORMAT "E%02iZ%02i"

/* mux */

struct s2p_595 global_s2p_595 = {
    .pin_serial_in = M1_PIN_595_IN,
    .pin_shift_clock = M1_PIN_595_SC,
    .pin_latch_clock = M1_PIN_595_LC,
    .pin_reset = M1_PIN_595_RST,
    .pin_output_enable = M1_PIN_595_OE,
};

/* consts */
static const char *str_e1_count = "e1_count";
static const int zones_per_extension_board = 4;

/* forward definitions */
static bool hw_m1e1_zone_set_init(struct ofp_hw *hw);
static bool hw_m1e1_zone_set_apply(struct ofp_hw *hw, struct tm *timeinfo);

/* hardware properties */
static struct ofp_hw hw_m1e1 = {
    .id = "M1E1",
    .description = "Based on a DevKit NodeMCU 30 pin and specialized mainboard M1 and expansion boards E1",
    .param_count = 1,
    .params = {
        {.id = "e1_count",
         .description = "Number of attached E1 boards",
         .type = HW_OFP_PARAM_TYPE_INTEGER,
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

/* init dynamic data and setup hardware (PLEASE READ ofp_hw_hooks in ofp.h) */
static bool hw_m1e1_zone_set_init(struct ofp_hw *hw)
{
    // esp_log_level_set(TAG, ESP_LOG_VERBOSE); // DEBUG

    ESP_LOGD(TAG, "hw_m1e1_zone_set_init %p", hw);
    assert(hw != NULL);

    // allocate memory for the total number of zones available
    struct ofp_hw_param *param_e1_count = ofp_hw_param_find_by_id(hw, str_e1_count);
    assert(param_e1_count != NULL);
    int zone_count = param_e1_count->value.int_ * zones_per_extension_board;
    if (!ofp_zone_set_allocate(&hw->zone_set, zone_count))
    {
        ESP_LOGW(TAG, "Could not allocate %i zones", zone_count);
        return false;
    }

    // setup zone names & descriptions
    char buf[OFP_MAX_LEN_VALUE];
    for (int i = 0; i < hw->zone_set.count; i++)
    {
        int e_num, z_num;
        e_num = i / zones_per_extension_board + 1;
        z_num = i % zones_per_extension_board + 1;
        int res = snprintf(buf, sizeof(buf), HW_M1E1_ZONE_ID_FORMAT, e_num, z_num);
        if (res < 0 || res > sizeof(buf))
        {
            ESP_LOGW(TAG, "Zone id too long (for %i)", i);
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

    // initialize hardware
    s2p_595_setup(&global_s2p_595);
    ofp_pin_setup_input_no_pull(M1_PIN_CONFIG);

    return true;
}

/* apply dynamic state to hardware */
static bool hw_m1e1_zone_set_apply(struct ofp_hw *hw, struct tm *timeinfo)
{
    ESP_LOGD(TAG, "hw_m1e1_zone_set_apply %p", hw);
    assert(hw != NULL);

    // push zone current state last to first
    s2p_595_reset(&global_s2p_595);
    for (int i = hw->zone_set.count - 1; i >= 0; i--)
    {
        struct ofp_zone *zone = &hw->zone_set.zones[i];

        bool pos, neg;
        if (!ofp_order_to_half_waves(zone->current, &pos, &neg, timeinfo))
            continue;

        ESP_LOGV(TAG, "index %i name %s min %i sec %i current %i pos %i neg %i", i, zone->id, timeinfo->tm_min, timeinfo->tm_sec, zone->current, pos, neg);

        /*
            From highest-numbered board (furthest from M board) to lowest-numbered board (nearest to M board)
            From highest-numbered fil-pilote (FPx) to lowest-numbered fil-pilote (FPx)
            First push N then P

            And for each board
            595 P0 = FP1P
            595 P1 = FP1N
            595 P2 = FP2P
            595 P3 = FP2N
            595 P4 = FP3P
            595 P5 = FP3N
            595 P6 = FP4P
            595 P7 = FP4N
        */
        s2p_595_set_input(&global_s2p_595, neg);
        s2p_595_shift_edge(&global_s2p_595);
        s2p_595_set_input(&global_s2p_595, pos);
        s2p_595_shift_edge(&global_s2p_595);
    }
    s2p_595_latch_edge(&global_s2p_595);

    return false;
}