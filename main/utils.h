#include <stdio.h>

void time_to_localtime(time_t *val, struct tm *timeinfo);
void localtime_to_string(struct tm *timeinfo, char *buf, int buf_len);
void display_current_localtime(const char *tag);