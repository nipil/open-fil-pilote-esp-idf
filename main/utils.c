#include <stdio.h>
#include <time.h>

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