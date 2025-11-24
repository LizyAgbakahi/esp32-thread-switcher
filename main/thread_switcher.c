#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

// -----------------------------
// User-level "thread" data
// -----------------------------

typedef struct
{
    const char *name;         // Name for logging
    uint64_t period_us;       // Desired period in microseconds
    uint64_t next_release_us; // Next time this thread should run
    uint64_t last_run_us;     // Last time it actually ran
    int id;                   // Simple ID
} user_thread_t;

// Forward declarations of "user thread" bodies
static void user_thread_A_body(user_thread_t *t);
static void user_thread_B_body(user_thread_t *t);

// Each entry = TCB + function pointer
typedef struct
{
    user_thread_t tcb;
    void (*func)(user_thread_t *t);
} user_thread_entry_t;

// For now, we just have 2 user-level threads
#define NUM_USER_THREADS 2
static user_thread_entry_t g_threads[NUM_USER_THREADS];

// -----------------------------
// Init user-level threads
// -----------------------------

static void init_user_threads(void)
{
    uint64_t now = esp_timer_get_time(); // current time in microseconds

    g_threads[0].tcb.name = "UA";
    g_threads[0].tcb.id = 0;
    g_threads[0].tcb.period_us = 500000;    // 500 ms
    g_threads[0].tcb.next_release_us = now; // ready to run immediately
    g_threads[0].tcb.last_run_us = 0;
    g_threads[0].func = user_thread_A_body;

    g_threads[1].tcb.name = "UB";
    g_threads[1].tcb.id = 1;
    g_threads[1].tcb.period_us = 500000; // 500 ms
    g_threads[1].tcb.next_release_us = now;
    g_threads[1].tcb.last_run_us = 0;
    g_threads[1].func = user_thread_B_body;
}

// -----------------------------
// Simple round-robin scheduler
// -----------------------------

static void scheduler_task(void *param)
{
    (void)param;
    printf("Starting user-level scheduler (round-robin over 2 threads)...\n");

    init_user_threads();

    int current_index = 0;

    while (1)
    {
        uint64_t now = esp_timer_get_time();

        // Try up to NUM_USER_THREADS entries in simple round-robin
        bool ran_any = false;
        for (int i = 0; i < NUM_USER_THREADS; i++)
        {
            int idx = (current_index + i) % NUM_USER_THREADS;
            user_thread_entry_t *entry = &g_threads[idx];
            user_thread_t *t = &entry->tcb;

            if (now >= t->next_release_us)
            {
                uint64_t delta = (t->last_run_us == 0)
                                     ? 0
                                     : now - t->last_run_us;

                printf("[UThread %s] now=%llu us, delta=%llu us (~%.2f ms)\n",
                       t->name,
                       (unsigned long long)now,
                       (unsigned long long)delta,
                       (delta == 0 ? 0.0 : (double)delta / 1000.0));

                t->last_run_us = now;
                t->next_release_us = now + t->period_us;

                // "Run" the user-level thread body
                entry->func(t);

                // Next time, start looking from the next index
                current_index = (idx + 1) % NUM_USER_THREADS;
                ran_any = true;
                break;
            }
        }

        // Avoid busy spinning: yield to FreeRTOS for ~1 ms
        if (!ran_any)
        {
            vTaskDelay(1); // 1 tick
        }
    }
}

// -----------------------------
// User-level thread bodies
// -----------------------------

static void user_thread_A_body(user_thread_t *t)
{
    // Simulate some "work"
    volatile int sum = 0;
    for (int i = 0; i < 1000; i++)
    {
        sum += i;
    }
    printf("  -> [UThread %s] did some work (sum loop)\n", t->name);
}

static void user_thread_B_body(user_thread_t *t)
{
    // Simulate different "work"
    volatile int prod = 1;
    for (int i = 1; i < 200; i++)
    {
        prod = (prod * i) % 100003;
    }
    printf("  -> [UThread %s] did some work (prod loop)\n", t->name);
}

// -----------------------------
// app_main: entry point
// -----------------------------

void app_main(void)
{
    printf("Starting Thread Switcher: User-level scheduler on ESP32 + FreeRTOS\n");

    // One FreeRTOS task that *is* our scheduler
    xTaskCreate(
        scheduler_task,
        "scheduler_task",
        4096, // stack size
        NULL, // param
        5,    // priority (medium-high)
        NULL  // handle
    );
}
