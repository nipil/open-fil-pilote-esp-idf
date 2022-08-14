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

const char TAG_UPTIME[] = "uptime";

/* global variables */
time_t system_start = 0;
time_t last_time = 0;

/* calculate corrected system uptime */
time_t uptime_get_time()
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

bool uptime_sync_check()
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

        ESP_LOGI(TAG_UPTIME, "Clock leap detected (delta=%+li), uptime changed accordingly.", delta);
        return true;
    }

    return false;
}

/* corrects system_time_start upon SNTP synchronization */
void uptime_sync_task(void *pvParameter)
{
    for (;;)
    {
        uptime_sync_check();
        vTaskDelay(pdMS_TO_TICKS(UPTIME_LOOP_WAIT_MILLISECONDS));
    }
}

/* starts compensation task */
void uptime_sync_start()
{
    // TODO: reduce stack size to maximum possible
    xTaskCreatePinnedToCore(uptime_sync_task, "sync_system_task", 2048, NULL, 1, NULL, 1);
}