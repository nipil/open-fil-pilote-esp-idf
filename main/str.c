#include <esp_http_server.h>
#include <nvs.h>

/* constants for efficient memory management */

const char *parse_int_re_str = "^(-|\\+)?([[:digit:]]+)$";

const char *default_nvs_partition_name = NVS_DEFAULT_PART_NAME;

const char *null_str = "NULL";

const char *stor_key_hardware_type = "hardware_type";

const char *json_key_current = "current";
const char *json_key_description = "description";
const char *json_key_id = "id";
const char *json_key_parameters = "parameters";
const char *json_key_supported = "supported";
const char *json_key_type = "type";
const char *json_key_value = "value";

const char *json_type_number = "number";
const char *json_type_string = "string";

const char *stor_key_zone_override = "override";
const char *stor_key_id = "id";
const char *stor_key_name = "name";
const char *stor_key_class = "class";

const char *stor_val_none = "none";

const char *http_content_type_html = HTTPD_TYPE_TEXT;
const char *http_content_type_js = "text/javascript";
const char *http_content_type_json = HTTPD_TYPE_JSON;

const char *route_root = "/";
const char *route_ofp_html = "/ofp.html";
const char *route_ofp_js = "/ofp.js";

const char *route_api_hardware = "^/ofp-api/v([[:digit:]]+)/hardware$";
const char *route_api_hardware_id_parameters = "^/ofp-api/v([[:digit:]]+)/hardware/([[:alnum:]]+)/parameters$";

const char *route_api_accounts = "^/ofp-api/v([[:digit:]]+)/accounts$";
const char *route_api_accounts_id = "^/ofp-api/v([[:digit:]]+)/accounts/([[:alnum:]]+)$";

const char *route_api_orders = "^/ofp-api/v([[:digit:]]+)/orders$";
const char *route_api_override = "^/ofp-api/v([[:digit:]]+)/override$";
const char *route_api_zones = "^/ofp-api/v([[:digit:]]+)/zones$";
const char *route_api_zones_id = "^/ofp-api/v([[:digit:]]+)/zones/([[:alnum:]]+)$";

const char *route_api_upgrade = "^/ofp-api/v([[:digit:]]+)/upgrade$";
const char *route_api_status = "^/ofp-api/v([[:digit:]]+)/status$";
const char *route_api_reboot = "^/ofp-api/v([[:digit:]]+)/reboot$";

const char *route_api_plannings = "^/ofp-api/v([[:digit:]]+)/plannings$";
const char *route_api_planning_id = "^/ofp-api/v([[:digit:]]+)/plannings/([[:digit:]]+)$";
const char *route_api_planning_id_slots = "^/ofp-api/v([[:digit:]]+)/plannings/([[:digit:]]+)/slots$";
const char *route_api_planning_id_slots_id = "^/ofp-api/v([[:digit:]]+)/plannings/([[:digit:]]+)/slots/((2[0-3]|[0-1][[:digit:]])h[0-5][[:digit:]])$";

const char *http_302_hdr = "302 Found";

const char *http_location_hdr = "Location";
