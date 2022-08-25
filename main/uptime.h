#ifndef UPTIME_H
#define UPTIME_H

#include <time.h>

#define UPTIME_LEAP_TRIGGER_SECONDS (5)
#define UPTIME_LOOP_WAIT_MILLISECONDS (1000)

struct uptime_wifi
{
    time_t cumulated_uptime;
    time_t last_connect_time;
    int attempts;
    int successes;
    int disconnects;
};

/* return corrected system uptime */
time_t uptime_get_time(void);

/* start system uptime compensation due to SNTP */
void uptime_sync_start(void);

/* allow to request an immediate check (returns true if clock changed) */
bool uptime_sync_check(void);

/* computes actual uptime in seconds */
time_t get_system_uptime(void);

/* wifi tracking functions */
const struct uptime_wifi *uptime_get_wifi_stats(void);
void uptime_track_wifi_attempt(void);
void uptime_track_wifi_success(void);
void uptime_track_wifi_disconnect(void);

#endif /* UPTIME_H */