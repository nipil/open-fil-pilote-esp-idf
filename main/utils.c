#include <stdio.h>
#include <time.h>
#include <regex.h>
#include <string.h>
#include <esp_log.h>

#include "utils.h"

static const char *TAG = "utils";

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
    int n = regerror(res, re, NULL, 0);
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
    ESP_LOGD(TAG, "re_str=%s str=%s", re_str, str);

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
    ESP_LOGD(TAG, "nmatch=%d", nmatch++);

    // alloc
    regmatch_t *pmatch = malloc(nmatch * sizeof(regmatch_t));
    assert(pmatch != NULL);

    // match
    res = regexec(&re, str, nmatch, pmatch, 0);
    ESP_LOGD(TAG, "regexec=%d", res);
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
        ESP_LOGD(TAG, "smatch[%d]=%s", i, smatch[i]);
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