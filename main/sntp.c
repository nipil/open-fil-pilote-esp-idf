#include <stdio.h>
#include <time.h>
#include <esp_log.h>
#include <esp_sntp.h>

#include "sntp.h"

#include "utils.h"
#include "uptime.h"

const char TAG_SNTP[] = "sntp";

void sntp_task_callback(struct timeval *tv)
{
    time_t now;
    struct tm timeinfo;
    char buf[40];

    /* notify uptime handler ASAP to handle possible clock jump */
    uptime_sync_check();

    /* track SNTP messages */
    ESP_LOGI(TAG_SNTP, "SNTP received time since epoch : = %jd sec: %li ms: %li us",
             (intmax_t)tv->tv_sec,
             tv->tv_usec / 1000,
             tv->tv_usec % 1000);

    /* log new localized time */
    time(&now);
    time_to_localtime(&now, &timeinfo);
    localtime_to_string(&timeinfo, buf, sizeof(buf));
    ESP_LOGI(TAG_SNTP, "Current time is : %s", buf);
}

void sntp_task_start()
{
    /* start network time synchronization */
    sntp_set_time_sync_notification_cb(sntp_task_callback);
    sntp_setservername(0, "pool.ntp.org");
    ESP_LOGI(TAG_SNTP, "Starting SNTP time configuration!");
    sntp_init();
}

void sntp_task_stop()
{
    ESP_LOGI(TAG_SNTP, "Stopping SNTP synchronization");
    sntp_stop();
}