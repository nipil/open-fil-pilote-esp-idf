#ifndef OFP_H
#define OFP_H

/* limits */
#define OFP_MAX_ZONE_COUNT 64
#define OFP_MAX_PLANNING_COUNT 32
#define OFP_MAX_PLANNING_SLOT_COUNT 64
#define OFP_MAX_HARDWARE_COUNT 4

#define OFP_MAX_LEN_ID 16
#define OFP_MAX_LEN_VALUE 32
#define OFP_MAX_LEN_DESCRIPTION 128
#define OFP_MAX_LEN_PLANNING_START 6

/* orders */

enum ofp_order_id // do NOT EVER change the numerical IDs
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

enum ofp_zone_mode // do NOT EVER change the numerical IDs
{
    HW_OFP_ZONE_MODE_FIXED = 0,
    HW_OFP_ZONE_MODE_PLANNING = 1,
    HW_OFP_ZONE_MODE_ENUM_SIZE
} ofp_zone_mode;

union ofp_zone_mode_data
{
    int planning_id;
    enum ofp_order_id order_id;
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

struct ofp_hw_hooks
{
    /*
     * Before this hook is called, each hardware parameter
     * has already been updated from NVS storage, so values
     * in memory are clearly defined.
     *
     * Then, this function is responsible for :
     * - calling ofp_zone_set_allocate to allocate the desired number of zones
     * - and setting the id and description for each zone
     * - do the "pure electronics hardware" initialization, if any
     *
     * This function MUST NOT do the following tasks :
     * - updating zone mode
     * - updating zone current state
     * As these will be done by the caller after initialization is successful
     */
    ofp_hw_func init;
    ofp_hw_func apply;
};

/* hardware */

enum ofp_hw_param_type // do NOT EVER change the numerical IDs
{
    HW_OFP_PARAM_TYPE_INTEGER = 0,
    HW_OFP_PARAM_TYPE_STRING = 1,
    HW_OFP_PARAM_TYPE_ENUM_SIZE
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
    struct ofp_hw_hooks hw_hooks;
    // configurable params
    int param_count;
    struct ofp_hw_param params[];
};

struct ofp_hw_list
{
    int hw_count;
    struct ofp_hw *hw[OFP_MAX_HARDWARE_COUNT];
};

/* plannings */

struct ofp_planning_slot
{
    char id_start[OFP_MAX_LEN_PLANNING_START];
    enum ofp_order_id order_id;
};

struct ofp_planning
{
    int id;
    char *description;
    // Fixed size, so no 'count' member
    struct ofp_planning_slot *slots[OFP_MAX_PLANNING_SLOT_COUNT];
};

struct ofp_planning_list
{
    int max_id;
    // Fixed size, so no 'count' member
    struct ofp_planning *plannings[OFP_MAX_PLANNING_COUNT];
};

/* order accessors */
const struct ofp_order_info *ofp_order_info_by_num_id(enum ofp_order_id order_id);
const struct ofp_order_info *ofp_order_info_by_str_id(char *order_id);
bool ofp_order_id_is_valid(enum ofp_order_id order_id);

/* enable an hardware implementation to be used */
void ofp_hw_register(struct ofp_hw *hw);

/* gives the number of registered definitions */
int ofp_hw_list_get_count(void);

/* hardware definition accessors */
struct ofp_hw *ofp_hw_list_get_hw_by_index(int n);
struct ofp_hw *ofp_hw_list_find_hw_by_id(char *hw_id);

/* hardware params accessors */
struct ofp_hw_param *ofp_hw_param_find_by_id(struct ofp_hw *hw, const char *param_id);
bool ofp_hw_param_set_value_string(struct ofp_hw_param *param, const char *str);

/* zone accessors */
bool ofp_zone_set_id(struct ofp_zone *zone, const char *id);
bool ofp_zone_set_description(struct ofp_zone *zone, const char *description);
bool ofp_zone_set_mode_fixed(struct ofp_zone *zone, enum ofp_order_id order_id);
bool ofp_zone_set_mode_planning(struct ofp_zone *zone, int planning_id);

/* allocate new space for zones set */
bool ofp_zone_set_allocate(struct ofp_zone_set *zone_set, int zone_count);

/* returns the global instance for the initialized hardware */
struct ofp_hw *ofp_hw_get_current(void);

/* initialize the hardware based on stored hardware id */
void ofp_hw_initialize(void);

/* update current orders for the zones and apply them to the hardware */
void ofp_hw_update(struct ofp_hw *hw);

/* planning accessors */
struct ofp_planning *ofp_planning_find_by_id(int planning_id);
struct ofp_planning_list *ofp_planning_list_get(void);
void ofp_planning_list_init(void);
bool ofp_planning_list_add_planning(struct ofp_planning *planning);

struct ofp_planning *ofp_planning_create(char *description);
bool ofp_planning_add_slot(struct ofp_planning *planning, struct ofp_planning_slot *slot);
struct ofp_planning_slot *ofp_planning_slot_create(int hour, int minute, enum ofp_order_id order_id);
#endif /* OFP_H */