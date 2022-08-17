#include <stddef.h>

#include "hw_m1e1.h"

static struct ofp_hw hw_m1e1 = {
    .id = "M1E1",
    .description = "Based on a DevKit NodeMCU 30 pin and specialized mainboard M1 and expansion boards E1",
    .param_count = 1,
    .params = {
        {.id = "e1_count",
         .description = "Number of attached E1 boards",
         .type = HW_OFP_PARAM_INTEGER,
         .value = {.int_ = 0}}}};

struct ofp_hw *hw_m1e1_get_definition(void)
{
    return &hw_m1e1;
}