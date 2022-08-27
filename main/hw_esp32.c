#include <stddef.h>
#include <esp_log.h>
#include "str.h"
#include "hw_esp32.h"

static const char TAG[] = "esp32";

/* consts */
static const char *str_zone_count = "zone_count";

/* forward definitions */
static bool hw_esp32_zone_set_init(struct ofp_hw *hw);
static bool hw_esp32_zone_set_apply(struct ofp_hw *hw);

/* hardware properties */
static struct ofp_hw hw_esp32 = {
    .id = "ESP32",
    .description = "Any ESP32, to target a new hardware or debug software without special hardware",
    .param_count = 2,
    .params = {
        {
            .id = "zone_prefix",
            .description = "this is a parameter of type string",
            .type = HW_OFP_PARAM_STRING,
            .value = {.string_ = "z"},
        },
        {
            .id = "zone_count",
            .description = "Nombre de zones",
            .type = HW_OFP_PARAM_INTEGER,
            .value = {.int_ = 0},
        },
    },
    .zone_set = {
        .count = 0,
        .zones = NULL,
    },
    .hw_hooks = {
        .init = hw_esp32_zone_set_init,
        .apply = hw_esp32_zone_set_apply,
    },
};

/* used for registering available hardware */
struct ofp_hw *hw_esp32_get_definition(void)
{
    return &hw_esp32;
}

/* init dynamic data and setup hardware */
static bool hw_esp32_zone_set_init(struct ofp_hw *hw)
{
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);

    ESP_LOGD(TAG, "hw_esp32_zone_set_init %p", hw);
    assert(hw != NULL);

    // allocate memory for the total number of zones available
    struct ofp_hw_param *param_zone_count = ofp_hw_param_find_by_id(hw, str_zone_count);
    assert(param_zone_count != NULL);
    int zone_count = param_zone_count->value.int_;
    ofp_zone_set_allocate(&hw->zone_set, zone_count);
    // TODO: setup zone names & descriptions

    /*
        INFO: implémenter ici l'initialisation de vos cartes
        et hardware spécifique au démarrage du matériel
    */
    return false;
}

/* apply dynamic state to hardware */
static bool hw_esp32_zone_set_apply(struct ofp_hw *hw)
{
    ESP_LOGD(TAG, "hw_esp32_zone_set_apply %p", hw);
    assert(hw != NULL);
    /*
        INFO: implémenter ici le fonctionnement de vos cartes
        et hardware spécifique, quand l'état des zones doit
        être appliqué sur les radiateurs des zones
    */
    return false;
}