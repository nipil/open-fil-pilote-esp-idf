#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "uptime.h"

static const char TAG[] = "uptime";

/* global variables */
static time_t system_start = 0;
static time_t last_time = 0;

/* calculate corrected system uptime */
time_t uptime_get_time(void)
{
    portMUX_TYPE mutex_system_start = portMUX_INITIALIZER_UNLOCKED;

    time_t now, uptime;
    time(&now);

    /* critical section for system_start */
    taskENTER_CRITICAL(&mutex_system_start);
    uptime = now - system_start;
    taskEXIT_CRITICAL(&mutex_system_start);

    return uptime;
}

bool uptime_sync_check(void)
{
    portMUX_TYPE mutex_system_start = portMUX_INITIALIZER_UNLOCKED;
    portMUX_TYPE mutex_last_time = portMUX_INITIALIZER_UNLOCKED;
    time_t now, delta;

    time(&now);

    /* critical section for last_time_seen */
    taskENTER_CRITICAL(&mutex_last_time);
    delta = now - last_time;
    last_time = now;
    taskEXIT_CRITICAL(&mutex_last_time);

    // leap detected, take into account
    if (abs(delta) > UPTIME_LEAP_TRIGGER_SECONDS)
    {
        /* critical section for system_start */
        taskENTER_CRITICAL(&mutex_system_start);
        system_start += delta;
        taskEXIT_CRITICAL(&mutex_system_start);

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