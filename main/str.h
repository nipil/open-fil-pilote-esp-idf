#ifndef STR_H
#define STR_H

const char *parse_alnum_re_str;
const char *parse_int_re_str;
const char *parse_zone_mode_re_str;
const char *parse_stored_password_re_str;
const char *parse_authorization_401_re_str;
const char *parse_credentials_401_re_str;

const char *default_nvs_partition_name;

const char *null_str;
const char *admin_str;

const char *http_get;
const char *http_post;
const char *http_put;
const char *http_patch;
const char *http_delete;

const char *stor_key_hardware_type;

const char *json_key_current;
const char *json_key_description;
const char *json_key_id;
const char *json_key_accounts;
const char *json_key_password;
const char *json_key_parameters;
const char *json_key_supported;
const char *json_key_type;
const char *json_key_value;
const char *json_key_mode;
const char *json_key_plannings;
const char *json_key_orders;
const char *json_key_slots;
const char *json_key_order;
const char *json_key_dow;
const char *json_key_hour;
const char *json_key_minute;

const char *json_type_number;
const char *json_type_string;

const char *stor_key_zone_override;
const char *stor_key_id;
const char *stor_key_name;
const char *stor_key_class;

const char *stor_val_none;

const char *http_content_type_html;
const char *http_content_type_js;
const char *http_content_type_json;

const char *str_cache_control;
const char *str_private_max_age_600;
const char *str_private_no_store;
const char *str_application_octet_stream;

const char *route_root;
const char *route_ofp_html;
const char *route_ofp_js;

const char *route_api_hardware;
const char *route_api_hardware_id_parameters;

const char *route_api_accounts;
const char *route_api_accounts_id;

const char *route_api_orders;
const char *route_api_override;
const char *route_api_zones;
const char *route_api_zones_id;

const char *route_api_upgrade;
const char *route_api_status;
const char *route_api_reboot;

const char *route_api_plannings;
const char *route_api_planning_id;
const char *route_api_planning_id_slots;
const char *route_api_planning_id_slots_id;

const char *http_302_hdr;
const char *http_401_hdr;

const char *http_location_hdr;
const char *http_authorization_hdr;
const char *http_www_authenticate_hdr;
const char *http_content_type_hdr;

#endif /* STR_H */