#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h" // for esp_timer_get_time()

// 500 ms period in ticks
#define TASK_PERIOD_MS 500

void taskA(void *param)
{
    int64_t last_time_us = esp_timer_get_time();

    while (1)
    {
        vTaskDelay(TASK_PERIOD_MS / portTICK_PERIOD_MS);

        int64_t now_us = esp_timer_get_time();
        int64_t delta_us = now_us - last_time_us;

        printf("[Task A] now=%lld us, delta=%lld us (~%lld ms)\n",
               now_us, delta_us, delta_us / 1000);

        last_time_us = now_us;
    }
}

void taskB(void *param)
{
    int64_t last_time_us = esp_timer_get_time();

    while (1)
    {
        vTaskDelay(TASK_PERIOD_MS / portTICK_PERIOD_MS);

        int64_t now_us = esp_timer_get_time();
        int64_t delta_us = now_us - last_time_us;

        printf("[Task B] now=%lld us, delta=%lld us (~%lld ms)\n",
               now_us, delta_us, delta_us / 1000);

        last_time_us = now_us;
    }
}

void app_main(void)
{
    printf("Starting Thread Switcher Demo...\n");

    // Create the two tasks
    xTaskCreate(taskA, "TaskA", 2048, NULL, 1, NULL);
    xTaskCreate(taskB, "TaskB", 2048, NULL, 1, NULL);
}
