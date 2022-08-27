#ifndef OFP_H
#define OFP_H

/* limits */
#define OFP_MAX_SIMULTANEOUS_HARDWARE 4
#define OFP_MAX_LEN_ID 16
#define OFP_MAX_LEN_VALUE 32
#define OFP_MAX_LEN_DESCRIPTION 128

/* orders, DO NOT EVER MODIFY THE IDs */

enum ofp_order_id
{
    // every heater understands these four
    HW_OFP_ORDER_ID_STANDARD_OFFLOAD = 0,
    HW_OFP_ORDER_ID_STANDARD_NOFREEZE = 1,
    HW_OFP_ORDER_ID_STANDARD_ECONOMY = 2,
    HW_OFP_ORDER_ID_STANDARD_COZY = 3,
    // more recent heaters understands these two as well
    HW_OFP_ORDER_ID_EXTENDED_COZYMINUS2 = 4,
    HW_OFP_ORDER_ID_EXTENDED_COZYMINUS1 = 5,
    HW_OFP_ORDER_ID_ENUM_SIZE
} ofp_order_id;

struct ofp_order_info
{
    char id[OFP_MAX_LEN_ID];
    char name[OFP_MAX_LEN_VALUE];
    char class[OFP_MAX_LEN_ID];
};

/* zones */

enum ofp_zone_mode
{
    HW_OFP_ZONE_MODE_FIXED = 0,
    HW_OFP_ZONE_MODE_PLANNING = 1,
    HW_OFP_ZONE_MODE_SIZE
} ofp_zone_mode;

union ofp_zone_mode_data
{
    int planning_id;
    enum ofp_zone_mode fixed_id;
};

struct ofp_zone
{
    char id[OFP_MAX_LEN_ID];
    char description[OFP_MAX_LEN_DESCRIPTION];
    enum ofp_zone_mode mode;
    union ofp_zone_mode_data mode_data;
    enum ofp_order_id current;
};

struct ofp_zone_set
{
    int count;
    struct ofp_zone *zones;
};

/* polymorphism */

struct ofp_hw; // forward declaration
typedef bool (*ofp_hw_func)(struct ofp_hw *hw);

struct ofp_hw_func
{
    ofp_hw_func init;
    ofp_hw_func apply;
};

/* hardware */

enum ofp_hw_param_type
{
    HW_OFP_PARAM_INTEGER = 0,
    HW_OFP_PARAM_STRING = 1,
};

union ofp_hw_param_value
{
    int int_;
    char string_[OFP_MAX_LEN_VALUE];
};

struct ofp_hw_param
{
    const char id[OFP_MAX_LEN_ID];
    char description[OFP_MAX_LEN_DESCRIPTION];
    enum ofp_hw_param_type type;
    union ofp_hw_param_value value;
};

struct ofp_hw
{
    const char id[OFP_MAX_LEN_ID];
    char description[OFP_MAX_LEN_DESCRIPTION];
    // dynamic zone data
    struct ofp_zone_set zone_set;
    // hooks
    struct ofp_hw_func hw_func;
    // configurable params
    int param_count;
    struct ofp_hw_param params[];
};

struct ofp_hw_list
{
    int hw_count;
    struct ofp_hw *hw[OFP_MAX_SIMULTANEOUS_HARDWARE];
};

/* accessors */
const struct ofp_order_info *ofp_order_info_by_num_id(enum ofp_order_id order_id);
const struct ofp_order_info *ofp_order_info_by_str_id(char *order_id);

/* enable an hardware implementation to be used */
void ofp_hw_register(struct ofp_hw *hw);

/* gives the number of registered definitions */
int ofp_hw_list_get_count(void);

/* get hardware definition by index */
struct ofp_hw *ofp_hw_list_get_hw_by_index(int n);

/* get hardware definition by id */
struct ofp_hw *ofp_hw_list_find_hw_by_id(char *hw_id);

#endif /* OFP_H */