#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void taskA(void *param)
{
    while (1)
    {
        printf("Running Task A\n");
        vTaskDelay(500 / portTICK_PERIOD_MS); // 500 ms delay
    }
}

void taskB(void *param)
{
    while (1)
    {
        printf("Running Task B\n");
        vTaskDelay(500 / portTICK_PERIOD_MS); // 500 ms delay
    }
}

void app_main(void)
{
    printf("Starting Thread Switcher Demo...\n");

    // Create the two tasks
    xTaskCreate(taskA, "TaskA", 2048, NULL, 1, NULL);
    xTaskCreate(taskB, "TaskB", 2048, NULL, 1, NULL);
}
