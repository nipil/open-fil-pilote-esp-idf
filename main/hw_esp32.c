#include <stddef.h>

#include "hw_esp32.h"

static struct ofp_hw hw_esp32 = {
    .id = "ESP32",
    .description = "Any ESP32, to target a new hardware or debug software without special hardware",
    .param_count = 2,
    .params = {
        {.id = "sample_param",
         .description = "this is a parameter of type string",
         .type = HW_OFP_PARAM_STRING,
         .value = {.string_ = "foo"}},
        {.id = "another_param",
         .description = "this is a parameter of type integer",
         .type = HW_OFP_PARAM_INTEGER,
         .value = {.int_ = 42}}}}; // Mark end of Flexible array

struct ofp_hw *hw_esp32_get_definition(void)
{
    return &hw_esp32;
}