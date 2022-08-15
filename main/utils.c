#include <stdio.h>

/* converts time to localtime using timezone */
void time_to_localtime(time_t *val, struct tm *timeinfo)
{
	setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); // TODO: migrate TZ definition to menuconfig
	tzset();
	localtime_r(val, timeinfo);
}

/* format localtime */
void localtime_to_string(struct tm *timeinfo, char *buf, int buf_len)
{
	strftime(buf, buf_len, "%c %z %Z", timeinfo);
}