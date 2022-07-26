#include <esp_http_server.h>
#include <nvs.h>

/* constants for efficient memory management */

const char *parse_alnum_re_str = "^([[:alnum:]]+)$";
const char *parse_int_re_str = "^(-|\\+)?([[:digit:]]+)$";
const char *parse_zone_mode_re_str = "^:((fixed):([[:alnum:]]+)|(planning):([[:digit:]]+))$";
const char *parse_stored_password_re_str = "^([[:digit:]]+):([[:digit:]]+):([[:alnum:]+/=]+):([[:alnum:]+/=]+)$"; // int:int:base64:base64
const char *parse_authorization_401_re_str = "^Basic ([[:alnum:]+/=]+)$";
const char *parse_credentials_401_re_str = "^([[:alnum:]]+):([[:alnum:]]+)$";

const char *default_nvs_partition_name = NVS_DEFAULT_PART_NAME;

const char *null_str = "NULL";
const char *admin_str = "admin";

const char *http_get = "GET";
const char *http_post = "POST";
const char *http_put = "PUT";
const char *http_patch = "PATCH";
const char *http_delete = "DELETE";

const char *stor_key_hardware_type = "hardware_type";

const char *json_key_current = "current";
const char *json_key_description = "description";
const char *json_key_id = "id";
const char *json_key_accounts = "accounts";
const char *json_key_password = "password";
const char *json_key_parameters = "parameters";
const char *json_key_supported = "supported";
const char *json_key_type = "type";
const char *json_key_value = "value";
const char *json_key_mode = "mode";
const char *json_key_plannings = "plannings";
const char *json_key_orders = "orders";
const char *json_key_slots = "slots";
const char *json_key_order = "order";
const char *json_key_dow = "dow";
const char *json_key_hour = "hour";
const char *json_key_minute = "minute";
const char *json_key_source_ip = "source_ip";

const char *json_type_number = "number";
const char *json_type_string = "string";

const char *stor_key_zone_override = "override";
const char *stor_key_id = "id";
const char *stor_key_name = "name";
const char *stor_key_class = "class";

const char *stor_key_https_certs = "https_certs";
const char *stor_key_https_key = "https_key";

const char *stor_val_none = "none";

const char *http_content_type_html = HTTPD_TYPE_TEXT;
const char *http_content_type_js = "text/javascript";
const char *http_content_type_json = HTTPD_TYPE_JSON;

const char *str_cache_control = "Cache-Control";
const char *str_private_max_age_600 = "private, max-age=600";
const char *str_private_no_store = "private, no-store";
const char *str_application_octet_stream = "application/octet-stream";
const char *str_application_x_pem_file = "application/x-pem-file";

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
const char *route_api_certificate = "^/ofp-api/v([[:digit:]]+)/certificate$";
const char *route_api_certificate_self_signed = "^/ofp-api/v([[:digit:]]+)/certificate/selfsigned$";

const char *route_api_plannings = "^/ofp-api/v([[:digit:]]+)/plannings$";
const char *route_api_planning_id = "^/ofp-api/v([[:digit:]]+)/plannings/([[:digit:]]+)$";
const char *route_api_planning_id_slots = "^/ofp-api/v([[:digit:]]+)/plannings/([[:digit:]]+)/slots$";
const char *route_api_planning_id_slots_id = "^/ofp-api/v([[:digit:]]+)/plannings/([[:digit:]]+)/slots/([[:digit:]]+)$";

const char *http_302_hdr = "302 Found";
const char *http_401_hdr = "401 Unauthorized";

const char *http_location_hdr = "Location";
const char *http_authorization_hdr = "Authorization";
const char *http_www_authenticate_hdr = "WWW-Authenticate";
const char *http_content_type_hdr = "Content-Type";

const char *pem_cert_begin = "-----BEGIN CERTIFICATE-----";
const char *pem_cert_end = "-----END CERTIFICATE-----";
const char *pem_unencrypted_key_begin = "-----BEGIN PRIVATE KEY-----";
const char *pem_unencrypted_key_end = "-----END PRIVATE KEY-----";
