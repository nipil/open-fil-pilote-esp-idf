#include <stdio.h>
#include <time.h>
#include <regex.h>
#include <string.h>
#include <esp_log.h>

#include "utils.h"

static const char *TAG = "utils";

static const char parse_int_re_str[] = "^(-|\\+)?([[:digit:]]+)$";

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
    // ESP_LOGD(TAG, "re_str=%s str=%s", re_str, str);

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
    // ESP_LOGD(TAG, "nmatch=%d", nmatch);

    // alloc
    regmatch_t *pmatch = malloc(nmatch * sizeof(regmatch_t));
    assert(pmatch != NULL);

    // match
    res = regexec(&re, str, nmatch, pmatch, 0);
    // ESP_LOGD(TAG, "regexec=%d", res);
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

    // alloc
    char **smatch = malloc(nmatch * sizeof(char *));
    assert(smatch != NULL);

    // extract
    for (int i = 0; i < nmatch; i++)
    {
        regmatch_t *m = &pmatch[i];
        if (m->rm_so == -1)
        {
            smatch[i] = NULL;
            ESP_LOGD(TAG, "smatch[%d]=NULL", i);
            continue;
        }
        smatch[i] = substr(str, m->rm_so, m->rm_eo - m->rm_so); // MUST BE FREED BY CALLER
        // ESP_LOGD(TAG, "smatch[%d]=%s", i, smatch[i]);
    }

    // cleanup
    free(pmatch);

    // build result
    struct re_result *out = malloc(sizeof(struct re_result));
    assert(out != NULL);
    out->count = nmatch;
    out->strings = smatch; // MUST BE FREED BY CALLER
    return out;
}

/* frees the results from re_match */
void re_free(struct re_result *r)
{
    assert(r != NULL);
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
        struct split_result *empty = malloc(sizeof(struct split_result *));
        assert(empty != NULL);
        empty->count = 0;
        empty->strings = NULL;
        return empty;
    }

    // ESP_LOGD(TAG, "split input %s", str);

    // count separators to determine how much memory we need to allocate
    int count = 0;
    for (int i = 0; i < src_len; i++)
        if (str[i] == sep)
            count++;

    // N separators means N+1 strings
    count++;

    // allocate
    struct split_result *splits = malloc(sizeof(struct split_result *));
    assert(splits != NULL);
    splits->count = count;
    splits->strings = malloc(count * sizeof(char *));
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
        // ESP_LOGD(TAG, "split_string %i is %s", current, sub);

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
        struct ofp_form_data *out = malloc(sizeof(struct ofp_form_data));
        assert(out != NULL);
        out->count = 0;
        out->params = NULL;
        split_string_free(params_raw);
        return out;
    }

    // allocate
    struct ofp_form_data *out = malloc(sizeof(struct ofp_form_data));
    assert(out != NULL);
    out->count = 0;
    out->params = malloc(params_raw->count * sizeof(struct ofp_form_param));
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
        struct re_result *res = re_match("^([^=]+)=([^=]*)$", param_raw_string); // MUST BE FREED BY CALLER

        // skip invalid params
        if (res == NULL || res->count != 3) // manually set capture count from param_re string
        {
            ESP_LOGW(TAG, "Skipping invalid part '%s' from application/x-www-form-urlencoded data '%s'", param_raw_string, data);
            if (res != NULL) // if it has unexpected number of captures
                re_free(res);
            continue;
        }

        // url-decode key and value
        struct ofp_form_param *target = &out->params[out->count];
        const char *key_enc = res->strings[1];
        char *key_dec = form_data_decode_str(key_enc);
        if (key_dec == NULL)
        {
            ESP_LOGW(TAG, "Skipping parameter %s: invalid url-encoded name '%s'", param_raw_string, key_enc);
            continue;
        }
        const char *val_enc = res->strings[2];
        char *val_dec = form_data_decode_str(val_enc);
        if (val_dec == NULL)
        {
            ESP_LOGW(TAG, "Skipping parameter '%s': invalid url-encoded value '%s'", param_raw_string, val_enc);
            continue;
        }

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
                // ESP_LOGD(TAG, "Decoding space");
                continue;
            }
            if (c == '%')
            {
                state = UD_PERCENT_FIRST;
                // ESP_LOGD(TAG, "Starting, Percent1");
                continue;
            }
            *out++ = c;
            // ESP_LOGD(TAG, "Copying 0x%02X %c", c, c);
            break;

        case UD_PERCENT_FIRST:
            tmp = hex_char_to_val(c);
            // ESP_LOGD(TAG, "hex_char_to_val %i", tmp);
            if (tmp == -1)
            {
                // ESP_LOGD(TAG, "Percent1 invalid character");
                free(decoded);
                return NULL;
            }
            value = tmp << 4;
            state = UD_PERCENT_SECOND;
            // ESP_LOGD(TAG, "Percent1, value 0x%02X, going Percent2", value);
            continue;

        case UD_PERCENT_SECOND:
            tmp = hex_char_to_val(c);
            // ESP_LOGD(TAG, "hex_char_to_val %i", tmp);
            if (tmp == -1)
            {
                // ESP_LOGD(TAG, "Percent2 invalid character");
                free(decoded);
                return NULL;
            }
            value += tmp;
            // ESP_LOGD(TAG, "Storing byte 0x%02X then normal", value);
            *out++ = value;
            state = UD_NORMAL;
            continue;
        }
    }
    // ESP_LOGD(TAG, "End loop");

    *out = '\0'; // final NULL terminator

    // ESP_LOGD(TAG, "Result: %s, original length %i final length %i", decoded, src_len, out - decoded);
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
bool parse_int(const char * str, int *target)
{
    assert(str != NULL);
    assert(target != NULL);

    ESP_LOGD(TAG, "input integer string: %s", str);
	struct re_result *res = re_match(parse_int_re_str, str);
    if (res == NULL) {
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