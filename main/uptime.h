#ifndef UPTIME_H
#define UPTIME_H

#include <stdio.h>

#define UPTIME_LEAP_TRIGGER_SECONDS (5)
#define UPTIME_LOOP_WAIT_MILLISECONDS (1000)

/* return corrected system uptime */
time_t uptime_get_time();

/* start system uptime compensation due to SNTP */
void uptime_sync_start();

/* allow to request an immediate check (returns true if clock changed) */
bool uptime_sync_check();

#endif /* UPTIME_H */