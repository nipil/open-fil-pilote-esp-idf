#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

// Some useful macros (source: linux kernel)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define sizeof_field(t, f) (sizeof(((t*)0)->f))

void time_to_localtime(time_t *val, struct tm *timeinfo);
void localtime_to_string(struct tm *timeinfo, char *buf, int buf_len);
void display_current_localtime(const char *tag);

#endif /* UTILS_H */