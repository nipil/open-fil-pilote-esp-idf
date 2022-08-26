#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "str.h"
#include "uptime.h"

static const char TAG[] = "uptime";

/* global variables */
static time_t system_start = 0;
static time_t last_time = 0;
static struct uptime_wifi wifi_stats = {0};

/* mutex variables */
portMUX_TYPE mutex_uptime_system_start = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE mutex_uptime_last_time = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE mutex_uptime_last_connect = portMUX_INITIALIZER_UNLOCKED;

/* calculate corrected system uptime */
time_t uptime_get_time(void)
{
    time_t now, uptime;
    time(&now);

    /* critical section for system_start */
    taskENTER_CRITICAL(&mutex_uptime_system_start);
    uptime = now - system_start;
    taskEXIT_CRITICAL(&mutex_uptime_system_start);

    return uptime;
}

/*
 * Applies the required time delta to the snapshotted boot time
 *
 * Call it periodically or whenever we detect a clock change
 */
bool uptime_sync_check(void)
{
    time_t now, delta;

    time(&now);

    /* critical section for last_time_seen */
    taskENTER_CRITICAL(&mutex_uptime_last_time);
    delta = now - last_time;
    last_time = now;
    taskEXIT_CRITICAL(&mutex_uptime_last_time);

    // leap detected, take into account
    if (abs(delta) > UPTIME_LEAP_TRIGGER_SECONDS)
    {
        /* critical section for system_start */
        taskENTER_CRITICAL(&mutex_uptime_system_start);
        system_start += delta;
        taskEXIT_CRITICAL(&mutex_uptime_system_start);

        /* critical section for wifi_stats.last_connect_time */
        taskENTER_CRITICAL(&mutex_uptime_last_connect);
        wifi_stats.last_connect_time += delta;
        taskEXIT_CRITICAL(&mutex_uptime_last_connect);

        ESP_LOGI(TAG, "Clock leap detected (delta=%+li), uptime changed accordingly.", delta);
        return true;
    }

    return false;
}

/* corrects system_time_start upon SNTP synchronization */
void uptime_sync_task(void *pvParameter)
{
    UBaseType_t min = 0xFFFF, max = -1, cur;

    for (;;)
    {
        uptime_sync_check();
        vTaskDelay(pdMS_TO_TICKS(UPTIME_LOOP_WAIT_MILLISECONDS));
        cur = uxTaskGetStackHighWaterMark(NULL);
        if (cur < min)
        {
            min = cur;
            ESP_LOGD(TAG, "StackHighWaterMark min changed: cur=%i, max=%i, min=%i", cur, max, min);
        }
        if (cur > max)
        {
            max = cur;
            ESP_LOGD(TAG, "StackHighWaterMark max changed: cur=%i, max=%i, min=%i", cur, max, min);
        }
    }
}

/* starts compensation task */
void uptime_sync_start(void)
{
    // TODO: reduce stack size to maximum possible
    // I (1548) uptime: StackHighWaterMark min changed: cur=1360, max=-1, min=1360
    // I (2548) uptime: StackHighWaterMark min changed: cur=432, max=-1, min=432
    xTaskCreatePinnedToCore(uptime_sync_task, "sync_system_task", 2048, NULL, 1, NULL, 1);
}

/* computes actual uptime in seconds */
time_t get_system_uptime(void)
{
    time_t now;
    time(&now);
    return now - system_start;
}

/* wifi tracking functions */
const struct uptime_wifi *uptime_get_wifi_stats(void)
{
    ESP_LOGV(TAG,
             "attempts %i, successes %i, disconnects %i, cumulated %li, last_connect %li",
             wifi_stats.attempts,
             wifi_stats.successes,
             wifi_stats.disconnects,
             wifi_stats.cumulated_uptime,
             wifi_stats.last_connect_time);

    return &wifi_stats;
}

void uptime_track_wifi_attempt(void)
{
    wifi_stats.attempts++;
    ESP_LOGV(TAG, "wifi uptime_track_wifi_attempt %i", wifi_stats.attempts);
}

void uptime_track_wifi_success(void)
{
    ESP_LOGV(TAG, "uptime_track_wifi_success");
    wifi_stats.successes++;

    time_t now;
    time(&now);
    ESP_LOGV(TAG, "last %li", now);

    /* critical section for wifi_stats.last_connect_time */
    taskENTER_CRITICAL(&mutex_uptime_last_connect);
    wifi_stats.last_connect_time = now;
    taskEXIT_CRITICAL(&mutex_uptime_last_connect);
}

void uptime_track_wifi_disconnect(void)
{
    ESP_LOGV(TAG, "uptime_track_wifi_disconnect");

    wifi_stats.disconnects++;

    /* critical section for wifi_stats.last_connect_time */
    taskENTER_CRITICAL(&mutex_uptime_last_connect);
    time_t tmp = wifi_stats.last_connect_time;
    taskEXIT_CRITICAL(&mutex_uptime_last_connect);

    if (tmp == 0)
        return;

    time_t now, delta;
    time(&now);

    /* critical section for wifi_stats.last_connect_time */
    taskENTER_CRITICAL(&mutex_uptime_last_connect);
    delta = now - wifi_stats.last_connect_time;
    wifi_stats.last_connect_time = 0;
    taskEXIT_CRITICAL(&mutex_uptime_last_connect);

    wifi_stats.cumulated_uptime += delta;
    ESP_LOGV(TAG, "delta %li", delta);
}