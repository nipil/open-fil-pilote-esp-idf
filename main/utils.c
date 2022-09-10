#include <stdio.h>
#include <time.h>
#include <regex.h>
#include <string.h>
#include <esp_log.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/base64.h>

#include "str.h"
#include "utils.h"

static const char *TAG = "utils";

/* defines */

#define PASSWORD_SALT_LENGTH 16 // max 16 bytes
#define PASSWORD_HASH_FUNCTION MBEDTLS_MD_SHA512
#define PASSWORD_HASH_ITERATIONS 4
#define INT32_MAX_DECIMAL_LENGTH 10

/* converts time to localtime using timezone */
void time_to_localtime(time_t *val, struct tm *timeinfo)
{
    setenv("TZ", CONFIG_OFP_LOCAL_TIMEZONE_SPEC, 1);
    tzset();
    localtime_r(val, timeinfo);
}

/* format localtime */
void localtime_to_string(struct tm *timeinfo, char *buf, int buf_len)
{
    strftime(buf, buf_len, "%c %z %Z", timeinfo);
}

/* display current localtime */
void log_current_localtime(const char *tag)
{
    time_t now;
    struct tm timeinfo;
    char buf[40];
    /* log new localized time */
    time(&now);
    time_to_localtime(&now, &timeinfo);
    localtime_to_string(&timeinfo, buf, sizeof(buf));
    ESP_LOGI(tag, "Current time is : %s", buf);
}

/* log regex error message */
void log_regerror(const char *TAG, regex_t *re, int res)
{
    int n = regerror(res, re, NULL, 0); // includes NULL terminator
    char *err = malloc(n);
    regerror(res, re, err, n);
    ESP_LOGE(TAG, "REGEX: %s", err);
    free((void *)err);
}

/* creates a copy of a substring, which MUST BE FREED BY THE CALLER */
char *substr(const char *src, int offset, int length)
{
    char *dest = malloc(length + 1);
    assert(dest != NULL);
    const char *begin = src + offset;
    strncpy(dest, begin, length);
    dest[length] = '\0';
    return dest; // must be free'd by the caller
}

/* Joins strings using separator, result MUST BE FREED BY THE CALLER */
char *joinstr_nargs(char *sep, int nargs, ...)
{
    va_list args;

    // separator
    int nsep = 0, len_sep = 0;
    if (sep && nargs > 1)
    {
        nsep = nargs - 1;
        len_sep = strlen(sep);
    }

    // memory
    int len = 0;
    va_start(args, nargs);
    for (int i = 0; i < nargs; i++)
    {
        len += strlen(va_arg(args, char *));
    }
    va_end(args);
    len += 1; // final NULL
    len += nsep;

    // concatenate
    char *buf = malloc(len);
    assert(buf != NULL);
    char *p = buf;

    va_start(args, nargs);
    for (int i = 0; i < nargs; i++)
    {
        // sep if needed
        if (sep && i > 0)
        {
            strcpy(p, sep);
            p += len_sep;
        }
        // source string
        char *a = va_arg(args, char *);
        strcpy(p, a);
        p += strlen(a);
    }
    va_end(args);

    *p = '\0';

    return buf; // MUST BE FREED BY CALLER
}

/*
 * Regex wrapper
 *
 * returns NULL on error or not found
 * caller has ownership of the returned structure
 * returned structure has ownership of the matches
 *
 * free everything using re_match_free
 */
struct re_result *re_match(const char *re_str, const char *str)
{
    ESP_LOGV(TAG, "re_str=%s str=%s", re_str, str);

    // compile
    regex_t re;
    int res = regcomp(&re, re_str, REG_EXTENDED);
    if (res != 0)
    {
        log_regerror(TAG, &re, res);
        return NULL;
    }

    // dummy attempt to compute the number of groups
    int nmatch = 0;
    bool last_escape = false;
    const char *tmp = re_str;
    while (*tmp)
    {
        // unescapted group begin
        if (*tmp == '(' && !last_escape)
            nmatch++;
        // in any case set/reset escape flag
        last_escape = (*tmp == '\\');
        // next character
        tmp++;
    }
    nmatch++; // for the whole match
    ESP_LOGV(TAG, "nmatch=%d", nmatch);

    // alloc and zero members
    regmatch_t *pmatch = calloc(nmatch, sizeof(regmatch_t));
    assert(pmatch != NULL);

    // match
    res = regexec(&re, str, nmatch, pmatch, 0);
    ESP_LOGV(TAG, "regexec=%d", res);
    if (res != 0)
    {
        if (res != REG_NOMATCH)
            log_regerror(TAG, &re, res);
        free(pmatch);
        regfree(&re);
        return NULL;
    }

    // cleanup
    regfree(&re);

    // alloc and zero members
    char **smatch = calloc(nmatch, sizeof(char *));
    assert(smatch != NULL);

    // extract
    for (int i = 0; i < nmatch; i++)
    {
        regmatch_t *m = &pmatch[i];
        if (m->rm_so == -1)
        {
            smatch[i] = NULL;
            ESP_LOGV(TAG, "smatch[%d]=NULL", i);
            continue;
        }
        smatch[i] = substr(str, m->rm_so, m->rm_eo - m->rm_so); // MUST BE FREED BY CALLER
        ESP_LOGV(TAG, "smatch[%d]=%s", i, smatch[i]);
    }

    // cleanup
    free(pmatch);

    // alloc and zero members
    struct re_result *out = calloc(1, sizeof(struct re_result));
    assert(out != NULL);

    // build result
    out->count = nmatch;
    out->strings = smatch; // MUST BE FREED BY CALLER
    return out;
}

/* frees the results from re_match */
void re_free(struct re_result *r)
{
    if (r == NULL) // behave as free() would
        return;

    if (!r) // if asserts are disabled
        return;
    if (!r->strings)
        return;
    for (int i = 0; i < r->count; i++)
    {
        free(r->strings[i]);
    }
    free(r->strings);
    free(r);
}

/* safe access and conversion */
int re_get_int(struct re_result *result, int index)
{
    assert(result != NULL);
    assert(index >= 0 && index < result->count);
    return atoi(result->strings[index]);
}

/* safe access */
char *re_get_string(struct re_result *result, int index)
{
    assert(result != NULL);
    assert(index >= 0 && index < result->count);
    return result->strings[index];
}

void split_string_free(struct split_result *splits)
{
    assert(splits != NULL);
    if (splits == NULL) // failsafe if asserts are disabled
        return;
    if (splits->strings != NULL)
    {
        free(splits->strings);
    }
    free(splits);
}

struct split_result *split_string(const char *str, char sep)
{
    assert(str != NULL);
    if (str == NULL) // fail safe if asserts are disabled
        return NULL;

    int src_len = strlen(str);

    /* An empty input string is not an error, just return an empty struct */
    if (src_len == 0)
    {
        ESP_LOGV(TAG, "split empty src");
        // alloc and zero members
        struct split_result *empty = calloc(1, sizeof(struct split_result *));
        assert(empty != NULL);
        empty->count = 0;
        empty->strings = NULL;
        return empty;
    }

    ESP_LOGV(TAG, "split input %s", str);

    // count separators to determine how much memory we need to allocate
    int count = 0;
    for (int i = 0; i < src_len; i++)
        if (str[i] == sep)
            count++;

    // N separators means N+1 strings
    count++;
    ESP_LOGV(TAG, "split sep count %i", count);

    // alloc and zero members
    struct split_result *splits = calloc(1, sizeof(struct split_result *));
    assert(splits != NULL);
    splits->count = count;

    // alloc and zero members
    splits->strings = calloc(count, sizeof(char *));
    assert(splits->strings != NULL);

    // extract
    int current = 0;
    int last = 0;
    for (int i = 0; i <= src_len /* capture last empty string too */; i++)
    {
        // guard clause to skip non-sep characters
        if (i < src_len && str[i] != sep)
            continue;

        // found separator, compute length
        int sub_len = i - last;
        char *sub = substr(str, last, sub_len); // must freed by caller
        ESP_LOGV(TAG, "split_string %i is %s", current, sub);

        // store
        splits->strings[current] = sub;
        current++;

        last = i + 1; // skip separator and restart from there
    }

    // verify that we captured everything
    assert(current == count);
    return splits;
}

/*
 * decode application/x-www-form-urlencoded form data
 * See https://www.w3.org/TR/html401/interact/forms.html#h-17.13.4.1
 */

void form_data_free(struct ofp_form_data *data)
{
    ESP_LOGV(TAG, "form_data_free %p", data);

    assert(data != NULL);
    if (data == NULL) // failsafe if asserts are disabled
        return;
    if (data->params != NULL)
    {
        for (int i = 0; i < data->count; i++)
        {
            struct ofp_form_param *param = &data->params[i];
            if (param->name != NULL)
                free(param->name);
            if (param->value != NULL)
                free(param->value);
        }
        free(data->params);
    }
    free(data);
}

struct ofp_form_data *form_data_parse(const char *data)
{
    assert(data != NULL);
    if (data == NULL) // failsafe if asserts are disabled
        return NULL;

    // extract param strings
    struct split_result *params_raw = split_string(data, '&');
    if (params_raw == NULL)
        return NULL;

    /*
     * Not having any params is not an error
     * Just return an empty list
     * This behaviour is similar to JavaScript
     */
    if (params_raw->count == 0)
    {
        ESP_LOGV(TAG, "form data parse no params");
        // alloc and zero members
        struct ofp_form_data *out = calloc(1, sizeof(struct ofp_form_data));
        assert(out != NULL);
        out->count = 0;
        out->params = NULL;
        split_string_free(params_raw);
        return out;
    }

    // alloc and zero members
    struct ofp_form_data *out = calloc(1, sizeof(struct ofp_form_data));
    assert(out != NULL);
    out->count = 0;
    out->params = calloc(params_raw->count, sizeof(struct ofp_form_param));
    assert(out->params != NULL);

    // split params into key/value pairs
    for (int i = 0; i < params_raw->count; i++)
    {
        // initialize default params values
        struct ofp_form_param *param = &out->params[i];
        param->name = NULL;
        param->value = NULL;

        // extract key/value pairs
        char *param_raw_string = params_raw->strings[i];
        ESP_LOGV(TAG, "form_data param_str %s", param_raw_string);
        struct re_result *res = re_match("^([^=]+)=([^=]*)$", param_raw_string); // MUST BE FREED BY CALLER

        // skip invalid params
        if (res == NULL || res->count != 3) // manually set capture count from param_re string
        {
            ESP_LOGD(TAG, "Skipping invalid part '%s' from application/x-www-form-urlencoded data '%s'", param_raw_string, data);
            re_free(res);
            continue;
        }

        // url-decode key and value
        struct ofp_form_param *target = &out->params[out->count];

        const char *key_enc = res->strings[1];
        ESP_LOGV(TAG, "form_data key_enc %s", key_enc);
        char *key_dec = form_data_decode_str(key_enc);
        if (key_dec == NULL)
        {
            ESP_LOGD(TAG, "Skipping parameter %s: invalid url-encoded name '%s'", param_raw_string, key_enc);
            continue;
        }
        ESP_LOGV(TAG, "form_data key_dec %s", key_dec);

        const char *val_enc = res->strings[2];
        ESP_LOGV(TAG, "form_data val_enc %s", val_enc);
        char *val_dec = form_data_decode_str(val_enc);
        if (val_dec == NULL)
        {
            ESP_LOGD(TAG, "Skipping parameter '%s': invalid url-encoded value '%s'", param_raw_string, val_enc);
            continue;
        }
        ESP_LOGV(TAG, "form_data val_dec %s", val_dec);

        // store
        target->name = key_dec;
        target->value = val_dec;
        out->count++;

        // cleanup re
        re_free(res);
    }

    // cleanup splits
    split_string_free(params_raw);

    // finally
    return out;
}

// returned value, if not NULL, MUST be freed by the caller */
char *form_data_decode_str(const char *str)
{
    // ESP_LOGD(TAG, "form_data_decode_str: %s", str ? str : "NULL");
    assert(str != NULL);
    if (str == NULL) // failsafe if asserts are disabled
    {
        return NULL;
    }

    int src_len = strlen(str);

    // decoded is shorter or as long as encoded source
    // include terminating NULL character
    char *decoded = malloc(src_len + 1); // MUST be freed by the caller
    char *out = decoded;

    // conversion statemachine
    enum url_decode_state
    {
        UD_NORMAL = 0,
        UD_PERCENT_FIRST = 1,
        UD_PERCENT_SECOND = 2
    };
    enum url_decode_state state = UD_NORMAL;

    char value;
    int tmp;
    for (int i = 0; i < src_len; i++)
    {
        char c = str[i];
        // ESP_LOGD(TAG, "state %i out %i index %i char %c", state, out - decoded, i, c);
        switch (state)
        {
        case UD_NORMAL:
            if (c == '+')
            {
                *out++ = ' ';
                ESP_LOGV(TAG, "Decoding space");
                continue;
            }
            if (c == '%')
            {
                state = UD_PERCENT_FIRST;
                ESP_LOGV(TAG, "Starting, Percent1");
                continue;
            }
            *out++ = c;
            ESP_LOGV(TAG, "Copying 0x%02X %c", c, c);
            break;

        case UD_PERCENT_FIRST:
            tmp = hex_char_to_val(c);
            ESP_LOGV(TAG, "hex_char_to_val %i", tmp);
            if (tmp == -1)
            {
                ESP_LOGV(TAG, "Percent1 invalid character");
                free(decoded);
                return NULL;
            }
            value = tmp << 4;
            state = UD_PERCENT_SECOND;
            ESP_LOGV(TAG, "Percent1, value 0x%02X, going Percent2", value);
            continue;

        case UD_PERCENT_SECOND:
            tmp = hex_char_to_val(c);
            ESP_LOGV(TAG, "hex_char_to_val %i", tmp);
            if (tmp == -1)
            {
                ESP_LOGV(TAG, "Percent2 invalid character");
                free(decoded);
                return NULL;
            }
            value += tmp;
            ESP_LOGV(TAG, "Storing byte 0x%02X then normal", value);
            *out++ = value;
            state = UD_NORMAL;
            continue;
        }
    }
    ESP_LOGV(TAG, "End loop");

    *out = '\0'; // final NULL terminator

    ESP_LOGD(TAG, "Result: %s, original length %i final length %i", decoded, src_len, out - decoded);
    return decoded;
}

/* searchs for a parameter name, and returns the value */
char *form_data_get_str(struct ofp_form_data *data, const char *name)
{
    assert(data != NULL);
    assert(name != NULL);

    for (int i = 0; i < data->count; i++)
        if (strcmp(data->params[i].name, name) == 0)
            return data->params[i].value;

    return NULL;
}

/* verifies format and convert */
bool parse_int(const char *str, int *target)
{
    assert(str != NULL);
    assert(target != NULL);

    ESP_LOGV(TAG, "input integer string: %s", str);
    struct re_result *res = re_match(parse_int_re_str, str);
    if (res == NULL)
    {
        return false;
    }

    // cleanup
    re_free(res);

    // return
    *target = atoi(str);
    return true;
}

/* Converts an hexadecimal digit to its value, or return -1 if invalid */
int hex_char_to_val(const char c)
{
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 0x0A;
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 0x0A;
    }
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    return -1;
}

/* wait functions */
void wait_ms(uint32_t ms)
{
    ESP_LOGV(TAG, "wait_ms %i", ms);
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void wait_sec(uint32_t sec)
{
    ESP_LOGV(TAG, "wait_sec %i", sec);
    vTaskDelay(pdMS_TO_TICKS(sec * 1000));
}

/* cJson helper functions */

enum json_helper_result cjson_get_child_int(cJSON *node, const char *key, int *target)
{
    assert(node != NULL);
    assert(key != NULL);
    assert(target != NULL);

    cJSON *j_target = cJSON_GetObjectItemCaseSensitive(node, key);
    if (j_target == NULL)
    {
        ESP_LOGD(TAG, "Missing JSON element '%s'", key);
        return JSON_HELPER_RESULT_NOT_FOUND;
    }
    if (!cJSON_IsNumber(j_target))
    {
        ESP_LOGD(TAG, "Invalid type or value for element %s", key);
        return JSON_HELPER_RESULT_INVALID;
    }
    *target = j_target->valueint;
    ESP_LOGV(TAG, "value: %i", *target);
    return JSON_HELPER_RESULT_SUCCESS;
}

enum json_helper_result cjson_get_child_string(cJSON *node, const char *key, char **target)
{
    // esp_log_level_set(TAG, ESP_LOG_VERBOSE); // DEBUG
    assert(node != NULL);
    assert(key != NULL);
    assert(target != NULL);

    cJSON *j_target = cJSON_GetObjectItemCaseSensitive(node, key);
    if (j_target == NULL)
    {
        ESP_LOGD(TAG, "Missing JSON element '%s'", key);
        *target = NULL;
        return JSON_HELPER_RESULT_NOT_FOUND;
    }
    if (!cJSON_IsString(j_target) || j_target->valuestring == NULL)
    {
        ESP_LOGD(TAG, "Invalid type or value for element %s", key);
        *target = NULL;
        return JSON_HELPER_RESULT_INVALID;
    }
    *target = j_target->valuestring;
    ESP_LOGV(TAG, "value %s", *target);
    return JSON_HELPER_RESULT_SUCCESS;
}

bool hmac_md(mbedtls_md_type_t md_type, const uint8_t *salt, size_t salt_len, const uint8_t *data, size_t data_len, uint8_t *output, uint8_t *output_len)
{
    ESP_LOGD(TAG, "hmac_md md_type %i salt %p salt_len %i data %p data_len %i output %p", md_type, salt, salt_len, data, data_len, output);

    if (salt == NULL || data == NULL || output == NULL)
        return false;

    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
    if (md_info == NULL)
    {
        ESP_LOGD(TAG, "mbedtls_md_info_from_type failed");
        return false;
    }

    mbedtls_md_context_t ctx;

    mbedtls_md_init(&ctx);

    int res = mbedtls_md_setup(&ctx, md_info, 1 /* hmac */);
    if (res != 0) // MBEDTLS_ERR_MD_BAD_INPUT_DATA, MBEDTLS_ERR_MD_ALLOC_FAILED
    {
        ESP_LOGD(TAG, "mbedtls_md_setup %i", res);
        return false;
    }

    ESP_LOGV(TAG, "salt");
    ESP_LOG_BUFFER_HEXDUMP(TAG, salt, salt_len, ESP_LOG_VERBOSE);
    mbedtls_md_hmac_starts(&ctx, (const uint8_t *)salt, salt_len);
    if (res != 0) // MBEDTLS_ERR_MD_BAD_INPUT_DATA
    {
        ESP_LOGD(TAG, "mbedtls_md_hmac_starts %i", res);
        return false;
    }

    ESP_LOGV(TAG, "data");
    ESP_LOG_BUFFER_HEXDUMP(TAG, data, data_len, ESP_LOG_VERBOSE);
    mbedtls_md_hmac_update(&ctx, (const uint8_t *)data, data_len);
    if (res != 0) // MBEDTLS_ERR_MD_BAD_INPUT_DATA
    {
        ESP_LOGD(TAG, "mbedtls_md_hmac_update %i", res);
        return false;
    }

    mbedtls_md_hmac_finish(&ctx, output);
    if (res != 0) // MBEDTLS_ERR_MD_BAD_INPUT_DATA
    {
        ESP_LOGD(TAG, "mbedtls_md_hmac_finish %i", res);
        return false;
    }
    mbedtls_md_free(&ctx);
    *output_len = mbedtls_md_get_size(md_info);

    ESP_LOGV(TAG, "hash_len %i", *output_len);
    ESP_LOGV(TAG, "hash");
    ESP_LOG_BUFFER_HEXDUMP(TAG, output, *output_len, ESP_LOG_VERBOSE);

    return true;
}

bool hmac_md_iterations(mbedtls_md_type_t md_type, const uint8_t *salt, size_t salt_len, const uint8_t *data, size_t data_len, uint8_t *output, uint8_t *output_len, size_t iterations)
{
    ESP_LOGD(TAG, "hmac_md_iterations md_type %i salt %p salt_len %i data %p data_len %i output %p iterations %i", md_type, salt, salt_len, data, data_len, output, iterations);

    if (iterations == 0)
    {
        ESP_LOGD(TAG, "invalid iterations %i", iterations);
        return false;
    }

    uint8_t hash_len;
    uint8_t dst_buf[MBEDTLS_MD_MAX_SIZE];
    ESP_LOGV(TAG, "dst_buf %p", dst_buf);

    // if single iteration, hash directly to output
    uint8_t *dst = (iterations == 1) ? output : dst_buf;

    // first iteration transforms from  variable-length input to a fixed-length output stored into dst
    if (!hmac_md(md_type, salt, salt_len, data, data_len, dst, &hash_len))
    {
        ESP_LOGD(TAG, "first hmac_md failed");
        return false;
    }

    // other iterations rehash the hash (using the salt too) by swapping src and dst each time
    uint8_t src_buf[MBEDTLS_MD_MAX_SIZE];
    ESP_LOGV(TAG, "src_buf %p", src_buf);
    uint8_t *src = src_buf, *tmp;
    while (--iterations > 0)
    {
        // swap
        tmp = src;
        src = dst;
        dst = (iterations == 1) ? output : tmp; // change target on last iteration
        ESP_LOGV(TAG, "src %p dst %p", src, dst);

        // re-hash
        if (!hmac_md(md_type, salt, salt_len, src, hash_len, dst, &hash_len))
        {
            ESP_LOGD(TAG, "other hmac_md failed");
            return false;
        }
    }

    // work is finished
    *output_len = hash_len;
    ESP_LOGD(TAG, "hmac_md_iterations finished output_len %i", *output_len);

    return true;
}

/*
 * Create a password string to be stored and used to verify password
 *
 * Format: int(mbedtls_md_type_t):int(iterations):base64(salt):base64(hash)
 *
 * Returned value (if not NULL) should be FREED BY CALLER
 */
char *password_string_create(char *cleartext)
{
    char *result = NULL;

    char *output_buf = NULL;

    ESP_LOGD(TAG, "password_create %p", cleartext);

    if (cleartext == NULL)
        goto cleanup;

    size_t cleartext_len = strlen(cleartext);
    ESP_LOGV(TAG, "cleartext_len=%d %s", cleartext_len, cleartext);

    mbedtls_md_type_t md_type = PASSWORD_HASH_FUNCTION;

    uint8_t salt[PASSWORD_SALT_LENGTH];
    esp_fill_random(salt, sizeof(salt));

    uint8_t hash_len;
    uint8_t hash[MBEDTLS_MD_MAX_SIZE];
    if (!hmac_md_iterations(md_type, salt, sizeof(salt), (const uint8_t *)cleartext, cleartext_len, hash, &hash_len, PASSWORD_HASH_ITERATIONS))
    {
        ESP_LOGD(TAG, "hmac_md failed");
        goto cleanup;
    }

    size_t base64_salt_len;
    mbedtls_base64_encode(NULL, 0, &base64_salt_len, salt, sizeof(salt));
    ESP_LOGV(TAG, "base64_salt_len %i", base64_salt_len);

    size_t base64_hash_len;
    mbedtls_base64_encode(NULL, 0, &base64_hash_len, hash, hash_len);
    ESP_LOGV(TAG, "base64_hash_len %i", base64_hash_len);

    size_t output_buf_len =
        INT32_MAX_DECIMAL_LENGTH   // algorithm id
        + 1                        // :
        + INT32_MAX_DECIMAL_LENGTH // iterations
        + 1                        // :
        + base64_salt_len - 1      // base64 without \0
        + 1                        // :
        + base64_hash_len - 1      // base64 without \0
        + 1;                       // \0
    ESP_LOGV(TAG, "output_buf_len %i", output_buf_len);

    output_buf = calloc(output_buf_len, sizeof(char));
    if (output_buf == NULL)
    {
        ESP_LOGD(TAG, "calloc error");
        goto cleanup;
    }

    char *output = output_buf;

    int n = snprintf(output, INT32_MAX_DECIMAL_LENGTH + 1, "%i", PASSWORD_HASH_FUNCTION);
    if (n < 0 || n >= INT32_MAX_DECIMAL_LENGTH + 1)
    {
        ESP_LOGD(TAG, "snprintf password_hash_func_id error");
        goto cleanup;
    }
    output += n;

    *output++ = ':';

    n = snprintf(output, INT32_MAX_DECIMAL_LENGTH + 1, "%i", PASSWORD_HASH_ITERATIONS);
    if (n < 0 || n >= INT32_MAX_DECIMAL_LENGTH + 1)
    {
        ESP_LOGD(TAG, "snprintf iterations error");
        goto cleanup;
    }
    output += n;

    *output++ = ':';

    size_t s;
    int res = mbedtls_base64_encode((uint8_t *)output, base64_salt_len, &s, salt, sizeof(salt));
    ESP_LOGV(TAG, "base64_encode salt res %i written %i", res, s);
    if (res != 0 || s + 1 != base64_salt_len) // MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL
    {
        ESP_LOGD(TAG, "base64_salt error");
        goto cleanup;
    }
    output += s;

    *output++ = ':';

    res = mbedtls_base64_encode((uint8_t *)output, base64_hash_len, &s, hash, hash_len);
    ESP_LOGV(TAG, "base64_encode hash res %i written %i", res, s);
    if (res != 0 || s + 1 != base64_hash_len) // MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL
    {
        ESP_LOGD(TAG, "base64_hash error");
        goto cleanup;
    }
    output += s;

    ESP_LOGV(TAG, "password_string %s", output_buf);
    return output_buf;

cleanup:
    free(output_buf);
    return result;
}

bool password_string_verify(char *cleartext, char *hashed)
{
    bool result = false;

    struct re_result *re = NULL;
    uint8_t *salt = NULL;
    uint8_t *hash = NULL;

    if (cleartext == NULL || hashed == NULL)
    {
        ESP_LOGW(TAG, "invalid input");
        goto cleanup;
    }

    size_t cleartext_len = strlen(cleartext);
    ESP_LOGV(TAG, "cleartext_len %d %s", cleartext_len, cleartext);

    re = re_match(parse_stored_password_re_str, hashed);
    ESP_LOGV(TAG, "re_match %p", re);
    if (re == NULL)
    {
        ESP_LOGW(TAG, "password hash does not match regex %s", parse_stored_password_re_str);
        goto cleanup;
    }

    mbedtls_md_type_t md_type = re_get_int(re, 1);
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
    if (md_info == NULL)
    {
        ESP_LOGD(TAG, "Unknown message digest type %i", md_type);
        goto cleanup;
    }

    int iterations = re_get_int(re, 2);
    if (iterations < 1)
    {
        ESP_LOGD(TAG, "Incorrect number of iterations %i", iterations);
        goto cleanup;
    }

    char *base64_salt = re_get_string(re, 3);
    char *base64_hash = re_get_string(re, 4);
    size_t base64_salt_len = strlen(base64_salt);
    size_t base64_hash_len = strlen(base64_hash);
    ESP_LOGV(TAG, "md_type %i iterations %i b64_salt %s b64_salt_len %i b64_hash %s b64_hash_len %i", md_type, iterations, base64_salt, base64_salt_len, base64_hash, base64_hash_len);

    size_t salt_len;
    mbedtls_base64_decode(NULL, 0, &salt_len, (uint8_t *)base64_salt, base64_salt_len);
    ESP_LOGV(TAG, "salt_len %i", salt_len);
    if (salt_len == 0)
    {
        ESP_LOGD(TAG, "Salt is empty");
        goto cleanup;
    }

    size_t hash_len;
    mbedtls_base64_decode(NULL, 0, &hash_len, (uint8_t *)base64_hash, base64_hash_len);
    ESP_LOGV(TAG, "hash_len %i", hash_len);
    if (hash_len != mbedtls_md_get_size(md_info))
    {
        ESP_LOGD(TAG, "Incorrect hash length %i", hash_len);
        goto cleanup;
    }

    salt = calloc(salt_len, sizeof(uint8_t));
    hash = calloc(hash_len, sizeof(uint8_t));
    if (salt == NULL || hash == NULL)
    {
        ESP_LOGD(TAG, "Cannot allocate memory for salt/hash");
        goto cleanup;
    }

    size_t s;
    int res = mbedtls_base64_decode(salt, salt_len, &s, (uint8_t *)base64_salt, base64_salt_len);
    ESP_LOGV(TAG, "base64_decode salt res %i written %i", res, s);
    if (res != 0 || s != salt_len) // MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL, MBEDTLS_ERR_BASE64_INVALID_CHARACTER
    {
        ESP_LOGD(TAG, "base64_decode salt error %i", res);
        goto cleanup;
    }
    ESP_LOG_BUFFER_HEXDUMP(TAG, salt, salt_len, ESP_LOG_VERBOSE);

    res = mbedtls_base64_decode(hash, hash_len, &s, (uint8_t *)base64_hash, base64_hash_len);
    ESP_LOGV(TAG, "base64_decode hash res %i written %i", res, s);
    if (res != 0 || s != hash_len) // MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL, MBEDTLS_ERR_BASE64_INVALID_CHARACTER
    {
        ESP_LOGD(TAG, "base64_decode hash error %i", res);
        goto cleanup;
    }
    ESP_LOG_BUFFER_HEXDUMP(TAG, hash, hash_len, ESP_LOG_VERBOSE);

    uint8_t tmp[MBEDTLS_MD_MAX_SIZE], tmp_len;
    if (!hmac_md_iterations(md_type, salt, salt_len, (const uint8_t *)cleartext, cleartext_len, tmp, &tmp_len, iterations) || tmp_len != hash_len)
    {
        ESP_LOGD(TAG, "hmac_md failed");
        goto cleanup;
    }

    res = memcmp(hash, tmp, hash_len);
    ESP_LOGV(TAG, "memcmp %i", res);

    result = (res == 0);

cleanup:
    free(hash);
    free(salt);
    re_free(re);
    return result;
}
