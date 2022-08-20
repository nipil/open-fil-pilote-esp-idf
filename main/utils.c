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
    const char *begin = src + offset;
    strncpy(dest, begin, length);
    dest[length] = '\0';
    return dest; // must be free'd by the caller
}

/* Joins strings using separator, result MUST BE FREED BY THE CALLER */
char *catstr_nargs(int nargs, ...)
{
    va_list args;

    // memory
    int len = 0;
    va_start(args, nargs);
    for (int i = 0; i < nargs; i++)
    {
        len += strlen(va_arg(args, char *));
    }
    va_end(args);
    len += 1; // final NULL

    // concatenate
    char *buf = malloc(len);
    char *p = buf;

    va_start(args, nargs);
    for (int i = 0; i < nargs; i++)
    {
        char *a = va_arg(args, char *);
        strcpy(p, a);
        p += strlen(a);
    }
    va_end(args);

    *p = '\0';

    return buf; // MUST BE FREED BY CALLER
}

/*
 * Tries to match an string to a regex dynamically built from the variable arguments and separator
 *
 * Use the macro if you want the pre-compiler to automatically compute the number of arguments
 * Use the function itself if/when you want to specify the number of arguments yourself
 *
 * Every variable argument must be a (char *)
 */
regex_t *join_and_match_re_nargs(const char *str, int nmatch, regmatch_t *pmatch, int nargs, ...)
{
    va_list args;

    // memory
    int len = 0;
    va_start(args, nargs);
    for (int i = 0; i < nargs; i++)
    {
        len += strlen(va_arg(args, char *));
    }
    va_end(args);
    len += 1; // final NULL

    // concatenate
    char *buf = malloc(len);
    char *p = buf;
    *p++ = '^';

    va_start(args, nargs);
    for (int i = 0; i < nargs; i++)
    {
        char *a = va_arg(args, char *);
        strcpy(p, a);
        p += strlen(a);
    }
    va_end(args);

    *p++ = '$';
    *p = '\0';
    ESP_LOGD(TAG, "REGEX prepared: %s", buf);

    // compile
    regex_t re;
    int res = regcomp(&re, buf, REG_EXTENDED);
    free((void *)buf);
    if (res != 0)
    {
        log_regerror(TAG, &re, res);
        assert(res == 0);
        return NULL;
    }

    // match
    res = regexec(&re, str, nmatch, pmatch, 0);
    switch (res)
    {
    case 0:
        // guard clause for found
        break;

    case REG_NOMATCH:
        // no match
        regfree(&re);
        return NULL;

    default:
        // error
        log_regerror(TAG, &re, res);
        regfree(&re);
        return NULL;
    }

    // list matches
    for (int i = 0; i < nmatch; i++)
    {
        regmatch_t *m = &pmatch[i];
        if (m->rm_so == -1)
            break;

        char *sub = substr(str, m->rm_so, m->rm_eo - m->rm_so);
        ESP_LOGD(TAG, "REGEX match %i start=%li end=%li: %s", i, m->rm_so, m->rm_eo, sub);
        free(sub);
    }

    return NULL;
}