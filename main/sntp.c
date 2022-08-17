#include <stdio.h>
#include <time.h>
#include <esp_log.h>
#include <esp_sntp.h>

#include "sntp.h"

#include "utils.h"
#include "uptime.h"

static const char TAG[] = "sntp";

void sntp_task_callback(struct timeval *tv)
{
    /* notify uptime handler ASAP to handle possible clock jump */
    uptime_sync_check();

    /* track SNTP messages */
    ESP_LOGD(TAG, "SNTP received time since epoch : = %jd sec: %li ms: %li us",
             (intmax_t)tv->tv_sec,
             tv->tv_usec / 1000,
             tv->tv_usec % 1000);

    /* display every time something intersting happens */
    display_current_localtime(TAG);
}

void sntp_task_start()
{
    /* display every time something intersting happens */
    display_current_localtime(TAG);

    /* start network time synchronization */
    sntp_set_time_sync_notification_cb(sntp_task_callback);
    sntp_setservername(0, CONFIG_OFP_SNTP_SERVER_NAME);
    ESP_LOGI(TAG, "Starting SNTP synchronization using %s", sntp_getservername(0));
    sntp_init();
}

void sntp_task_stop()
{
    ESP_LOGI(TAG, "Stopping SNTP synchronization");
    sntp_stop();
}