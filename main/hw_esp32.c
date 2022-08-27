#include <stddef.h>
#include <esp_log.h>
#include "str.h"
#include "hw_esp32.h"

static const char TAG[] = "esp32";

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
    .hw_func = {
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