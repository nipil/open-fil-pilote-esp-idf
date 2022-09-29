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
    char **smatch = calloc(nmatch, sizeof(char *)); // smatch is a array of strings (i.e. char *)
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
        struct split_result *empty = calloc(1, sizeof(struct split_result));
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
    struct split_result *splits = calloc(1, sizeof(struct split_result));
    assert(splits != NULL);
    splits->count = count;

    // alloc and zero members
    splits->strings = calloc(count, sizeof(char *)); // strings is a array of strings (i.e. char *)
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
char *password_to_string(struct password_data *pwd)
{
    ESP_LOGD(TAG, "password_to_string %p", pwd);

    char *output_buf = NULL;

    if (pwd == NULL)
        goto cleanup;

    size_t base64_salt_len;
    mbedtls_base64_encode(NULL, 0, &base64_salt_len, pwd->salt, pwd->salt_len);
    ESP_LOGV(TAG, "base64_salt_len %i", base64_salt_len);

    size_t base64_hash_len;
    mbedtls_base64_encode(NULL, 0, &base64_hash_len, pwd->hash, pwd->hash_len);
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
    int res = mbedtls_base64_encode((uint8_t *)output, base64_salt_len, &s, pwd->salt, pwd->salt_len);
    ESP_LOGV(TAG, "base64_encode salt res %i written %i", res, s);
    if (res != 0 || s + 1 != base64_salt_len) // MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL
    {
        ESP_LOGD(TAG, "base64_salt error");
        goto cleanup;
    }
    output += s;

    *output++ = ':';

    res = mbedtls_base64_encode((uint8_t *)output, base64_hash_len, &s, pwd->hash, pwd->hash_len);
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
    return NULL;
}

struct password_data *password_from_string(const char *str)
{
    // esp_log_level_set(TAG, ESP_LOG_VERBOSE); // DEBUG
    ESP_LOGD(TAG, "password_from_string %p", str);

    struct re_result *re = NULL;
    struct password_data *tmp = NULL;

    if (str == NULL)
        goto cleanup;

    tmp = calloc(1, sizeof(struct password_data));
    if (tmp == NULL)
        goto cleanup;

    tmp->md_type = MBEDTLS_MD_NONE;

    ESP_LOGV(TAG, "password_str %s", str);

    re = re_match(parse_stored_password_re_str, str);
    ESP_LOGV(TAG, "re_match %p", re);
    if (re == NULL)
    {
        ESP_LOGW(TAG, "password hash does not match regex %s", parse_stored_password_re_str);
        goto cleanup;
    }

    tmp->md_type = re_get_int(re, 1);
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(tmp->md_type);
    if (md_info == NULL)
    {
        ESP_LOGD(TAG, "Unknown message digest type %i", tmp->md_type);
        goto cleanup;
    }

    tmp->iterations = re_get_int(re, 2);
    if (tmp->iterations < 1)
    {
        ESP_LOGD(TAG, "Incorrect number of iterations %i", tmp->iterations);
        goto cleanup;
    }

    char *base64_salt = re_get_string(re, 3);
    char *base64_hash = re_get_string(re, 4);
    size_t base64_salt_len = strlen(base64_salt);
    size_t base64_hash_len = strlen(base64_hash);
    ESP_LOGV(TAG, "md_type %i iterations %i b64_salt %s b64_salt_len %i b64_hash %s b64_hash_len %i", tmp->md_type, tmp->iterations, base64_salt, base64_salt_len, base64_hash, base64_hash_len);

    mbedtls_base64_decode(NULL, 0, &tmp->salt_len, (uint8_t *)base64_salt, base64_salt_len);
    ESP_LOGV(TAG, "salt_len %i", tmp->salt_len);
    if (tmp->salt_len == 0)
    {
        ESP_LOGD(TAG, "Salt is empty");
        goto cleanup;
    }

    mbedtls_base64_decode(NULL, 0, &tmp->hash_len, (uint8_t *)base64_hash, base64_hash_len);
    ESP_LOGV(TAG, "hash_len %i", tmp->hash_len);
    if (tmp->hash_len != mbedtls_md_get_size(md_info))
    {
        ESP_LOGD(TAG, "Incorrect hash length %i", tmp->hash_len);
        goto cleanup;
    }

    tmp->salt = calloc(tmp->salt_len, sizeof(uint8_t));
    tmp->hash = calloc(tmp->hash_len, sizeof(uint8_t));
    if (tmp->salt == NULL || tmp->hash == NULL)
    {
        ESP_LOGD(TAG, "Cannot allocate memory for salt/hash");
        goto cleanup;
    }

    size_t s;
    int res = mbedtls_base64_decode(tmp->salt, tmp->salt_len, &s, (uint8_t *)base64_salt, base64_salt_len);
    ESP_LOGV(TAG, "base64_decode salt res %i written %i", res, s);
    if (res != 0 || s != tmp->salt_len) // MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL, MBEDTLS_ERR_BASE64_INVALID_CHARACTER
    {
        ESP_LOGD(TAG, "base64_decode salt error");
        goto cleanup;
    }
    ESP_LOG_BUFFER_HEXDUMP(TAG, tmp->salt, tmp->salt_len, ESP_LOG_VERBOSE);

    res = mbedtls_base64_decode(tmp->hash, tmp->hash_len, &s, (uint8_t *)base64_hash, base64_hash_len);
    ESP_LOGV(TAG, "base64_decode hash res %i written %i", res, s);
    if (res != 0 || s != tmp->hash_len) // MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL, MBEDTLS_ERR_BASE64_INVALID_CHARACTER
    {
        ESP_LOGD(TAG, "base64_decode hash error");
        goto cleanup;
    }
    ESP_LOG_BUFFER_HEXDUMP(TAG, tmp->hash, tmp->hash_len, ESP_LOG_VERBOSE);

    re_free(re);
    return tmp;

cleanup:
    password_free(tmp);
    re_free(re);
    return NULL;
}

void password_log(struct password_data *pwd, const char *tag, esp_log_level_t log_level)
{
    ESP_LOG_LEVEL_LOCAL(log_level, tag, "password type: %i", pwd->md_type);
    ESP_LOG_LEVEL_LOCAL(log_level, tag, "password iterations: %i", pwd->iterations);
    ESP_LOG_LEVEL_LOCAL(log_level, tag, "password salt_len: %i", pwd->salt_len);
    ESP_LOG_LEVEL_LOCAL(log_level, tag, "password salt: %p", pwd->salt);
    if (pwd->salt != NULL)
        ESP_LOG_BUFFER_HEXDUMP(tag, pwd->salt, pwd->salt_len, log_level);
    ESP_LOG_LEVEL_LOCAL(log_level, tag, "password hash_len: %u", pwd->hash_len);
    ESP_LOG_LEVEL_LOCAL(log_level, tag, "password hash: %p", pwd->hash);
    if (pwd->hash != NULL)
        ESP_LOG_BUFFER_HEXDUMP(tag, pwd->hash, pwd->hash_len, log_level);
}

bool password_free(struct password_data *pwd)
{
    ESP_LOGD(TAG, "password_free %p", pwd);
    if (pwd == NULL)
        return false;

    if (pwd->salt != NULL)
        free(pwd->salt);

    if (pwd->hash != NULL)
        free(pwd->hash);

    free(pwd);

    return true;
}

struct password_data *password_init(const char *cleartext)
{
    ESP_LOGD(TAG, "password_init %p", cleartext);

    struct password_data *tmp = NULL;

    if (cleartext == NULL)
        goto cleanup;

    tmp = calloc(1, sizeof(struct password_data));
    if (tmp == NULL)
        goto cleanup;

    size_t cleartext_len = strlen(cleartext);
    ESP_LOGV(TAG, "cleartext %i %s", cleartext_len, cleartext);

    tmp->md_type = PASSWORD_HASH_FUNCTION;
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(tmp->md_type);
    if (md_info == NULL)
        goto cleanup;

    tmp->hash_len = mbedtls_md_get_size(md_info);

    tmp->iterations = PASSWORD_HASH_ITERATIONS;
    if (tmp->iterations == 0)
        goto cleanup;

    tmp->salt_len = PASSWORD_SALT_LENGTH;
    if (tmp->salt_len == 0)
        goto cleanup;

    tmp->salt = malloc(tmp->salt_len);
    if (tmp->salt == NULL)
        goto cleanup;

    esp_fill_random(tmp->salt, tmp->salt_len);

    tmp->hash = malloc(tmp->hash_len);
    if (tmp->hash == NULL)
        goto cleanup;

    uint8_t out_len;
    if (!hmac_md_iterations(tmp->md_type, tmp->salt, tmp->salt_len, (const uint8_t *)cleartext, cleartext_len, tmp->hash, &out_len, tmp->iterations) || out_len != tmp->hash_len)
        goto cleanup;

    return tmp;

cleanup:
    password_free(tmp);
    return NULL;
}

bool password_verify(struct password_data *pwd, const char *cleartext)
{
    ESP_LOGD(TAG, "password_verify %p %p", pwd, cleartext);

    if (pwd == NULL || cleartext == NULL)
        goto cleanup;

    size_t cleartext_len = strlen(cleartext);
    ESP_LOGV(TAG, "cleartext %i %s", cleartext_len, cleartext);

    uint8_t tmp[MBEDTLS_MD_MAX_SIZE], tmp_len;
    if (!hmac_md_iterations(pwd->md_type, pwd->salt, pwd->salt_len, (const uint8_t *)cleartext, cleartext_len, tmp, &tmp_len, pwd->iterations) || tmp_len != pwd->hash_len)
        goto cleanup;

    ESP_LOG_BUFFER_HEXDUMP(TAG, tmp, tmp_len, ESP_LOG_VERBOSE);
    return (memcmp(pwd->hash, tmp, pwd->hash_len) == 0);

cleanup:
    return false;
}

int pem_parse_single_certificate(const char *cert_str, size_t cert_len_with_null, mbedtls_x509_crt *output)
{
    int result = MBEDTLS_ERR_X509_BAD_INPUT_DATA;

    if (output == NULL)
        goto cleanup;

    ESP_LOGD(TAG, "certificate_check_public_single cert_str %p cert_len_with_null %i output %p", cert_str, cert_len_with_null, output);
    ESP_LOGV(TAG, "%s", cert_str);

    mbedtls_x509_crt_init(output);

    result = mbedtls_x509_crt_parse(output, (const unsigned char *)cert_str, cert_len_with_null);
    ESP_LOGD(TAG, "mbedtls_x509_crt_parse result %i", result);
    if (result != 0)
        goto cleanup;

    char buf_dn[MBEDTLS_X509_MAX_DN_NAME_SIZE + 1];
    int result_dn = mbedtls_x509_dn_gets(buf_dn, sizeof(buf_dn), &output->subject);
    ESP_LOGD(TAG, "mbedtls_x509_dn_gets result %i", result_dn);
    if (result_dn < 0)
        goto cleanup;

    ESP_LOGI(TAG, "Successfully parsed certificate with Subject: %s", buf_dn);
    return result;

cleanup:
    ESP_LOGD(TAG, "Problem %i encountered while parsing certificate", result);
    return result;
}

int pem_parse_single_private_key(const char *key_str, size_t key_len_with_null, char *key_pass, size_t key_pass_len, mbedtls_pk_context *output)
{
    int result = MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;

    if (output == NULL)
        goto cleanup;

    ESP_LOGD(TAG, "certificate_check_private key_str %p key_len_with_null %i key_pass %p key_pass_len %i output %p", key_str, key_len_with_null, key_pass, key_pass_len, output);
    ESP_LOGV(TAG, "%s", key_str);

    mbedtls_pk_init(output);

    result = mbedtls_pk_parse_key(output, (const unsigned char *)key_str, key_len_with_null, (const unsigned char *)key_pass, key_pass_len);
    ESP_LOGD(TAG, "mbedtls_pk_parse_key result %i", result);
    if (result == MBEDTLS_ERR_PK_PASSWORD_REQUIRED)
    {
        ESP_LOGW(TAG, "Could not parse certificate key, password is required and none provided");
        goto cleanup;
    }
    if (result == MBEDTLS_ERR_PK_PASSWORD_MISMATCH)
    {
        ESP_LOGW(TAG, "Could not parse certificate key, provided password does not match");
        goto cleanup;
    }
    if (result != 0)
        goto cleanup;

    const char *key_name = mbedtls_pk_get_name(output);
    ESP_LOGI(TAG, "Successfully parsed private key of type: %s", key_name ? key_name : null_str);

    return result;

cleanup:
    ESP_LOGD(TAG, "Problem %i encountered while parsing private key", result);
    return result;
}

struct certificate_bundle_iter *certificate_bundle_iter_init(const void *buf, size_t len)
{
    if (buf == NULL)
        return NULL;

    ESP_LOGD(TAG, "certificate_bundle_iter_init buffer %p len %i", buf, len);

    struct certificate_bundle_iter *tmp = calloc(1, sizeof(struct certificate_bundle_iter));
    if (tmp == NULL)
        goto cleanup;

    tmp->current = buf;
    tmp->remaining = len;

    tmp->state = CBIS_IDLE;

    tmp->block_start = NULL;
    tmp->block_len = -1;

    return tmp;

cleanup:
    free(tmp);
    return NULL;
}

void certificate_bundle_iter_free(struct certificate_bundle_iter *it)
{
    ESP_LOGD(TAG, "certificate_bundle_iter_free it %p", it);
    free(it);
}

bool certificate_bundle_iter_next(struct certificate_bundle_iter *it)
{
    ESP_LOGD(TAG, "certificate_bundle_iter_next it %p", it);

    if (it == NULL)
        return false;

    ESP_LOGD(TAG, "state %i", it->state);
    if (it->state == CBIS_END_OK || it->state == CBIS_END_FAIL)
        return false;

    const uint8_t pem_cert_begin_len = strlen(pem_cert_begin);
    const uint8_t pem_cert_end_len = strlen(pem_cert_end);
    const uint8_t pem_unencrypted_key_begin_len = strlen(pem_unencrypted_key_begin);
    const uint8_t pem_unencrypted_key_end_len = strlen(pem_unencrypted_key_end);

    // search for a new block
    it->state = CBIS_IDLE;
    it->block_start = NULL;
    it->block_len = -1;

    ESP_LOGD(TAG, "remaining %i", it->remaining);
    if (it->remaining == 0)
    {
        it->state = CBIS_END_OK;
        return false;
    }

    for (int i = 0; i < 2; i++)
    {
        ESP_LOGD(TAG, "current %p remaining %i", it->current, it->remaining);

        char *next = memchr(it->current, '-', it->remaining);
        if (next == NULL)
        {
            ESP_LOGD(TAG, "Label marker not found");
            break;
        }

        it->remaining -= next - it->current;
        it->current = next;
        ESP_LOGD(TAG, "Found marker, remaining now %i, previewing current: %.*s", it->remaining, min_int(it->remaining, 30), it->current);

        switch (it->state)
        {
        case CBIS_IDLE:
            if (it->remaining >= pem_cert_begin_len && strncmp(it->current, pem_cert_begin, pem_cert_begin_len) == 0)
            {
                it->state = CBIS_CERTIFICATE;
                it->block_start = (char *)it->current;
                it->current += pem_cert_begin_len;
                it->remaining -= pem_cert_begin_len;
                break;
            }
            if (it->remaining >= pem_unencrypted_key_begin_len && strncmp(it->current, pem_unencrypted_key_begin, pem_unencrypted_key_begin_len) == 0)
            {
                it->state = CBIS_PRIVATE_KEY;
                it->block_start = (char *)it->current;
                it->current += pem_unencrypted_key_begin_len;
                it->remaining -= pem_unencrypted_key_begin_len;
                break;
            }
            ESP_LOGW(TAG, "Unknown opening label found, starting with: %.*s", min_int(it->remaining, 30), it->current);
            it->state = CBIS_END_FAIL;
            return false;

        case CBIS_CERTIFICATE:
            if (it->remaining >= pem_cert_end_len && strncmp(it->current, pem_cert_end, pem_cert_end_len) == 0)
            {
                it->current += pem_cert_end_len;
                it->remaining -= pem_cert_end_len;
                it->block_len = it->current - it->block_start;
                return true;
            }
            ESP_LOGW(TAG, "Incorrect closing label detected (expecting %s) but found something starting with: %.*s", pem_cert_end, min_int(it->remaining, 30), it->current);
            it->state = CBIS_END_FAIL;
            return false;

        case CBIS_PRIVATE_KEY:
            if (it->remaining >= pem_unencrypted_key_end_len && strncmp(it->current, pem_unencrypted_key_end, pem_unencrypted_key_end_len) == 0)
            {
                it->current += pem_unencrypted_key_end_len;
                it->remaining -= pem_unencrypted_key_end_len;
                it->block_len = it->current - it->block_start;
                return true;
            }
            // invalid token
            ESP_LOGW(TAG, "Incorrect closing label detected (expecting %s) but found something starting with: %.*s", pem_unencrypted_key_end, min_int(it->remaining, 30), it->current);
            it->state = CBIS_END_FAIL;
            return false;

        case CBIS_END_OK:
        case CBIS_END_FAIL:
            return false;
        }
    }

    it->state = (it->state == CBIS_IDLE) ? CBIS_END_OK : CBIS_END_FAIL;
    return false;
}

void certificate_bundle_iter_log(struct certificate_bundle_iter *it, const char *tag, esp_log_level_t level)
{
    if (it == NULL)
        return;

    ESP_LOG_LEVEL_LOCAL(level, tag, "cbis: current %p remaining %i state %i block_start %p block_len %i",
                        it->current,
                        it->remaining,
                        it->state,
                        it->block_start,
                        it->block_len);
}
