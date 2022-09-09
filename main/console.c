#include <string.h>
#include <esp_log.h>
#include <esp_console.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <argtable3/argtable3.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "sdkconfig.h"

#include "ofp.h"
#include "storage.h"

#define CONSOLE_MAX_COMMAND_LINE_LENGTH 512

static const char TAG[] = "console";

// 'version' command
static int get_version(int argc, char **argv)
{
    const char *model;
    esp_chip_info_t info;
    uint32_t flash_size;
    esp_chip_info(&info);

    switch (info.model)
    {
    case CHIP_ESP32:
        model = "ESP32";
        break;
    case CHIP_ESP32S2:
        model = "ESP32-S2";
        break;
    case CHIP_ESP32S3:
        model = "ESP32-S3";
        break;
    case CHIP_ESP32C3:
        model = "ESP32-C3";
        break;
    case CHIP_ESP32H2:
        model = "ESP32-H2";
        break;
    // case CHIP_ESP32C2:
    //     model = "ESP32-C2";
    //     break;
    default:
        model = "Unknown";
        break;
    }

    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK)
    {
        printf("Get flash size failed");
        return 1;
    }
    printf("IDF Version:%s\r\n", esp_get_idf_version());
    printf("Chip info:\r\n");
    printf("\tmodel:%s\r\n", model);
    printf("\tcores:%d\r\n", info.cores);
    printf("\tfeature:%s%s%s%s%d%s\r\n",
           info.features & CHIP_FEATURE_WIFI_BGN ? "/802.11bgn" : "",
           info.features & CHIP_FEATURE_BLE ? "/BLE" : "",
           info.features & CHIP_FEATURE_BT ? "/BT" : "",
           info.features & CHIP_FEATURE_EMB_FLASH ? "/Embedded-Flash:" : "/External-Flash:",
           flash_size / (1024 * 1024), " MB");
    printf("\trevision number:%d\r\n", info.revision);
    return 0;
}

static void register_version(void)
{
    const esp_console_cmd_t cmd = {
        .command = "version",
        .help = "Get version of chip and SDK",
        .hint = NULL,
        .func = &get_version,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// 'restart' command restarts the program

static int restart(int argc, char **argv)
{
    ESP_LOGI(TAG, "Restarting");
    esp_restart();
}

static void register_restart(void)
{
    const esp_console_cmd_t cmd = {
        .command = "restart",
        .help = "Software reset of the chip",
        .hint = NULL,
        .func = &restart,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// 'free' command prints available heap memory

static int free_mem(int argc, char **argv)
{
    printf("\r\n%d bytes\n", esp_get_free_heap_size());
    return 0;
}

static void register_free(void)
{
    const esp_console_cmd_t cmd = {
        .command = "free",
        .help = "Get the current size of free heap memory",
        .hint = NULL,
        .func = &free_mem,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// 'heap' command prints minumum heap size
static int heap_size(int argc, char **argv)
{
    uint32_t heap_size = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    printf("min heap size: %u bytes\n", heap_size);
    return 0;
}

static void register_heap(void)
{
    const esp_console_cmd_t heap_cmd = {
        .command = "heap",
        .help = "Get minimum size of free heap memory that was available during program execution",
        .hint = NULL,
        .func = &heap_size,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&heap_cmd));
}

// log_level command changes log level via esp_log_level_set

static struct
{
    struct arg_str *tag;
    struct arg_str *level;
    struct arg_end *end;
} log_level_args;

static const char *s_log_level_names[] = {
    "none",
    "error",
    "warn",
    "info",
    "debug",
    "verbose"};

static int log_level(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&log_level_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, log_level_args.end, argv[0]);
        return 1;
    }
    assert(log_level_args.tag->count == 1);
    assert(log_level_args.level->count == 1);
    const char *tag = log_level_args.tag->sval[0];
    const char *level_str = log_level_args.level->sval[0];
    esp_log_level_t level;
    size_t level_len = strlen(level_str);
    for (level = ESP_LOG_NONE; level <= ESP_LOG_VERBOSE; level++)
    {
        if (memcmp(level_str, s_log_level_names[level], level_len) == 0)
        {
            break;
        }
    }
    if (level > ESP_LOG_VERBOSE)
    {
        printf("Invalid log level '%s', choose from none|error|warn|info|debug|verbose\n", level_str);
        return 1;
    }
    if (level > CONFIG_LOG_MAXIMUM_LEVEL)
    {
        printf("Can't set log level to %s, max level limited in menuconfig to %s. "
               "Please increase CONFIG_LOG_MAXIMUM_LEVEL in menuconfig.\n",
               s_log_level_names[level], s_log_level_names[CONFIG_LOG_MAXIMUM_LEVEL]);
        return 1;
    }
    esp_log_level_set(tag, level);
    printf("\r\nLog level '%s' set for tag '%s'\n", level_str, tag);
    return 0;
}

static void register_log_level(void)
{
    log_level_args.tag = arg_str1(NULL, NULL, "<tag|*>", "Log tag to set the level for, or * to set for all tags");
    log_level_args.level = arg_str1(NULL, NULL, "<none|error|warn|debug|verbose>", "Log level to set. Abbreviated words are accepted.");
    log_level_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "log_level",
        .help = "Set log level for all tags or a specific tag.",
        .hint = NULL,
        .func = &log_level,
        .argtable = &log_level_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
static int task_stats(int argc, char **argv)
{
    printf("\r\n");
    volatile UBaseType_t n_tasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *buf = pvPortMalloc(n_tasks * sizeof(TaskStatus_t));
    if (buf == NULL)
        return -1;

    uint32_t total_runtime;
    n_tasks = uxTaskGetSystemState(buf, n_tasks, &total_runtime);
    if (n_tasks == 0)
    {
        free(buf);
        return -1;
    }

    total_runtime /= 100; // percentage
    printf("Num\tState\tPrio\tRun%%\tHWM\tCore\tName\r\n");
    char status[] = {'X', 'R', 'B', 'S', 'D', 'I'};
    for (int i = 0; i < n_tasks; i++)
    {
        TaskStatus_t *t = &buf[i];
        printf("%i\t%c\t%i\t%i\t%i\t%i\t%s\r\n",
               t->xTaskNumber,
               status[t->eCurrentState],
               t->uxCurrentPriority,
               total_runtime == 0 ? -1 : t->ulRunTimeCounter / total_runtime,
               t->usStackHighWaterMark,
               t->xCoreID == tskNO_AFFINITY ? -1 : t->xCoreID,
               t->pcTaskName);
    }

    free(buf);
    return 0;
}

static void register_task_stats(void)
{
    const esp_console_cmd_t cmd = {
        .command = "tasks",
        .help = "Show task stats",
        .hint = NULL,
        .func = &task_stats};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
#endif /* CONFIG_FREERTOS_USE_TRACE_FACILITY */

// 'partitions' command prints partition layout
static int show_partitions(int argc, char **argv)
{
    printf("\r\n");
    esp_partition_iterator_t it_p = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it_p != NULL)
    {
        const esp_partition_t *part = esp_partition_get(it_p);
        printf("Partition %s\r\n\tType: 0x%02X\r\n\tSubType: 0x%02X\r\n\tAddress: 0x%08X\r\n\tSize: %i\r\n\tEncrypted: %s\r\n",
               part->label,
               part->type,
               part->subtype,
               part->address,
               part->size,
               part->encrypted ? "yes" : "no");
        it_p = esp_partition_next(it_p);
    }
    return 0;
}

static void register_partitions(void)
{
    const esp_console_cmd_t cmd = {
        .command = "partitions",
        .help = "Show partitions",
        .hint = NULL,
        .func = &show_partitions,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// 'nvs_clear' command
static struct // argument order defined by struct ordering
{
    struct arg_str *target;
    struct arg_end *end;
} nvs_clear_args;

static int nvs_clear_cmd(int argc, char **argv)
{
    printf("\r\n");
    // esp_log_level_set(TAG, ESP_LOG_VERBOSE); // DEBUG
    int nerrors = arg_parse(argc, argv, (void **)&nvs_clear_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, nvs_clear_args.end, argv[0]);
        return 1;
    }

    const char *target = nvs_clear_args.target->sval[0];
    kv_ns_clear_atomic(target);
    printf("Namespace %s cleared\r\n", target);
    return 0;
}

static void register_nvs_clear(void)
{
    nvs_clear_args.target = arg_str1(NULL, NULL, "<ns>", "Namespace to clear");
    nvs_clear_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "nvs_clear",
        .help = "Clear all entries from a speicifc NVS namespace",
        .hint = NULL,
        .func = &nvs_clear_cmd,
        .argtable = &nvs_clear_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// 'nvs_delete' command
static struct // argument order defined by struct ordering
{
    struct arg_str *ns;
    struct arg_str *key;
    struct arg_end *end;
} nvs_delete_args;

static int nvs_delete_cmd(int argc, char **argv)
{
    printf("\r\n");
    // esp_log_level_set(TAG, ESP_LOG_VERBOSE); // DEBUG
    int nerrors = arg_parse(argc, argv, (void **)&nvs_delete_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, nvs_delete_args.end, argv[0]);
        return 1;
    }

    const char *ns = nvs_delete_args.ns->sval[0];
    const char *key = nvs_delete_args.key->sval[0];
    kv_ns_delete_atomic(ns, key);
    printf("Key %s deleted from namespace %s deleted\r\n", key, ns);
    return 0;
}

static void register_nvs_delete(void)
{
    nvs_delete_args.ns = arg_str1(NULL, NULL, "<ns>", "Namespace where the key is");
    nvs_delete_args.key = arg_str1(NULL, NULL, "<key>", "Key to delete");
    nvs_delete_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "nvs_delete",
        .help = "delete a specific entry from a specific NVS namespace",
        .hint = NULL,
        .func = &nvs_delete_cmd,
        .argtable = &nvs_delete_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// 'nvs' command prints nvs

static struct // argument order defined by struct ordering
{
    struct arg_str *ns;
    struct arg_end *end;
} nvs_show_args;

static int show_nvs(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&nvs_show_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, nvs_show_args.end, argv[0]);
        return 1;
    }
    const char *ns = nvs_show_args.ns->sval[0];
    if (strcmp(ns, "*") == 0)
        ns = NULL;

    printf("\r\n");
    esp_partition_iterator_t it_p = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    for (; it_p != NULL; it_p = esp_partition_next(it_p))
    {
        const esp_partition_t *part = esp_partition_get(it_p);
        if (part->type == ESP_PARTITION_TYPE_DATA && part->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS)
        {
            // list NVS partition stats
            nvs_stats_t nvs_stats;
            esp_err_t err = nvs_get_stats(part->label, &nvs_stats);
            ESP_LOGV(TAG, "nvs_get_stats: %s", esp_err_to_name(err));
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Storage stats failed: %s", esp_err_to_name(err));
                continue;
            }
            printf("Partition '%s' NVS stats: Used %d, Free = (%d), All = (%d)\r\n",
                   part->label, nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);

            // list NVS partition content
            nvs_iterator_t it = nvs_entry_find(part->label, NULL, NVS_TYPE_ANY);
            for (; it != NULL; it = nvs_entry_next(it))
            {
                nvs_entry_info_t info;
                nvs_entry_info(it, &info);
                if (ns && strcmp(info.namespace_name, ns) != 0)
                    continue;
                printf("\tnamespace '%s', key '%s' type", info.namespace_name, info.key);
                const char *str = kv_type_str_from_nvs_type(info.type);
                printf(" '%s'", str ? str : "?");

                char *tmp;
                char *blob;
                size_t len;
                printf(" value: ");
                switch (info.type)
                {
                case NVS_TYPE_U8:
                    printf("%i", kv_ns_get_u8_atomic(info.namespace_name, info.key, -1));
                    break;
                case NVS_TYPE_I8:
                    printf("%i", kv_ns_get_i8_atomic(info.namespace_name, info.key, -1));
                    break;
                case NVS_TYPE_U16:
                    printf("%i", kv_ns_get_u16_atomic(info.namespace_name, info.key, -1));
                    break;
                case NVS_TYPE_I16:
                    printf("%i", kv_ns_get_i16_atomic(info.namespace_name, info.key, -1));
                    break;
                case NVS_TYPE_U32:
                    printf("%i", kv_ns_get_u32_atomic(info.namespace_name, info.key, -1));
                    break;
                case NVS_TYPE_I32:
                    printf("%i", kv_ns_get_i32_atomic(info.namespace_name, info.key, -1));
                    break;
                case NVS_TYPE_U64:
                    printf("%llu", kv_ns_get_u64_atomic(info.namespace_name, info.key, -1));
                    break;
                case NVS_TYPE_I64:
                    printf("%lli", kv_ns_get_i64_atomic(info.namespace_name, info.key, -1));
                    break;
                case NVS_TYPE_STR:
                    tmp = kv_ns_get_str_atomic(info.namespace_name, info.key);
                    if (tmp != NULL)
                    {
                        printf("%s", tmp);
                        free(tmp);
                    }
                    else
                    {
                        printf("NULL");
                    }

                    break;
                case NVS_TYPE_BLOB:
                    blob = kv_ns_get_blob_atomic(info.namespace_name, info.key, &len);
                    if (blob != NULL)
                    {
                        printf("len=%i", len);
                        for (int i = 0; i < len; i++)
                        {
                            if (i % 16 == 0)
                                printf("\r\n\t\t");
                            printf("%02X ", blob[i]);
                        }
                        free(blob);
                    }
                    else
                    {
                        printf("NULL");
                    }

                    break;
                default:
                    break;
                }
                printf("\r\n");
            }
            nvs_release_iterator(it);
        };
    }
    printf("\r\n");
    return 0;
}

static void register_nvs(void)
{
    nvs_show_args.ns = arg_str1(NULL, NULL, "<*|ns>", "Namespace to list");
    nvs_show_args.end = arg_end(1);

    const esp_console_cmd_t cmd = {
        .command = "nvs",
        .help = "Show NVS",
        .hint = NULL,
        .func = &show_nvs,
        .argtable = &nvs_show_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// 'hardware' command prints in-memory hardware definition
static int show_hardware(int argc, char **argv)
{
    printf("\r\n");
    struct ofp_hw *hw = ofp_hw_get_current();
    if (hw == NULL)
    {
        printf("No hardware available\r\n");
        return -1;
    }

    printf("Hardware %s\r\n\t%s\r\n\tParams: %i\r\n", hw->id, hw->description, hw->param_count);
    for (int i = 0; i < hw->param_count; i++)
    {
        struct ofp_hw_param *param = &hw->params[i];
        printf("\t\tParam %s", param->id);
        switch (param->type)
        {
        case HW_OFP_PARAM_TYPE_INTEGER:
            printf(" type Integer value %i", param->value.int_);
            break;
        case HW_OFP_PARAM_TYPE_STRING:
            printf(" type String value %s", param->value.string_);
            break;
        default:
            printf(" type ?");
            break;
        }
        printf("\r\n");
    }

    return 0;
}

static void register_hardware(void)
{
    const esp_console_cmd_t cmd = {
        .command = "hardware",
        .help = "Show hardware configuration",
        .hint = NULL,
        .func = &show_hardware,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// 'zones' command prints in-memory zone configuration
static int show_zones(int argc, char **argv)
{
    printf("\r\n");
    struct ofp_hw *hw = ofp_hw_get_current();
    if (hw == NULL)
    {
        printf("No hardware available\r\n");
        return -1;
    }

    // override
    enum ofp_order_id order_id;
    bool active = ofp_override_get_order_id(&order_id);
    const struct ofp_order_info *info = NULL;
    if (active)
    {
        info = ofp_order_info_by_num_id(order_id);
        printf("Override active : order %s\r\n", info->id);
    }
    else
    {
        printf("Override inactive\r\n");
    }

    printf("Zone count: %i\r\n", hw->zone_set.count);
    for (int i = 0; i < hw->zone_set.count; i++)
    {
        const struct ofp_order_info *info;
        struct ofp_zone *zone = &hw->zone_set.zones[i];
        printf("\tZone %s (desc: %s) ", zone->id, zone->description);
        switch (zone->mode)
        {
        case HW_OFP_ZONE_MODE_FIXED:
            info = ofp_order_info_by_num_id(zone->mode_data.order_id);
            printf(" type Fixed value %s", info->id);
            break;
        case HW_OFP_ZONE_MODE_PLANNING:
            printf(" type Planning value %i", zone->mode_data.planning_id);
            break;
        default:
            printf(" type ?");
            break;
        }
        info = ofp_order_info_by_num_id(zone->current);
        printf(" current %s \r\n", info->id);
    }

    return 0;
}

static void register_zones(void)
{
    const esp_console_cmd_t cmd = {
        .command = "zones",
        .help = "Show zone configuration",
        .hint = NULL,
        .func = &show_zones,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// 'plannings' command prints in-memory zone configuration
static int show_plannings(int argc, char **argv)
{
    printf("\r\n");
    extern struct ofp_planning_list *plan_list_global;
    if (plan_list_global == NULL)
    {
        printf("No planning list available\r\n");
        return -1;
    }

    for (int i = 0; i < OFP_MAX_PLANNING_COUNT; i++)
    {
        struct ofp_planning *plan = plan_list_global->plannings[i];
        if (plan == NULL)
            continue;
        printf("Planning index %i holds a planning:\r\n\tID: %i\r\n\tDescription: %s\r\n",
               i,
               plan->id,
               plan->description);

        for (int j = 0; j < OFP_MAX_PLANNING_SLOT_COUNT; j++)
        {
            struct ofp_planning_slot *slot = plan->slots[j];
            if (slot == NULL)
                continue;

            printf("\t\tSlot index %i holds a slot: id %i dow %i hour %i minute %i order_id: %s\r\n",
                   j,
                   slot->id, slot->dow, slot->hour, slot->minute,
                   ofp_order_info_by_num_id(slot->order_id)->id);
        }
    }

    return 0;
}

static void register_plannings(void)
{
    const esp_console_cmd_t cmd = {
        .command = "plannings",
        .help = "Show plannings configuration",
        .hint = NULL,
        .func = &show_plannings,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

void console_init(void)
{
    // esp_log_level_set(TAG, ESP_LOG_VERBOSE); // DEBUG
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    repl_config.prompt = CONFIG_OFP_HOSTNAME ">";
    repl_config.max_cmdline_length = CONSOLE_MAX_COMMAND_LINE_LENGTH;

    /* Register commands */
    esp_console_register_help_command();
    register_version();
    register_log_level();
    register_free();
    register_heap();
    register_version();
    register_restart();
    register_log_level();
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
    register_task_stats();
#endif /* CONFIG_FREERTOS_USE_TRACE_FACILITY */
    register_hardware();
    register_zones();
    register_partitions();
    register_nvs();
    register_nvs_clear();
    register_nvs_delete();
    register_plannings();

    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}