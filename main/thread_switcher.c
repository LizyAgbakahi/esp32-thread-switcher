#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

// -----------------------------
// Scheduling policies
// -----------------------------
#define SCHED_POLICY_RR 0
#define SCHED_POLICY_EDF 1

// 🔽 Change this between SCHED_POLICY_RR and SCHED_POLICY_EDF
#define SCHED_POLICY SCHED_POLICY_EDF

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

    // Timing stats (observed periods)
    uint64_t min_delta_us;
    uint64_t max_delta_us;
    uint64_t sum_delta_us;
    uint32_t run_count;

    // Deadline stats
    uint32_t deadline_misses;   // how many times we ran "late"
    uint64_t worst_lateness_us; // largest (delta - period_us)
} user_thread_t;

// Forward declarations of "user thread" bodies
static void user_thread_A_body(user_thread_t *t);
static void user_thread_B_body(user_thread_t *t);
static void user_thread_C_body(user_thread_t *t); // NEW third thread

// Each entry = TCB + function pointer
typedef struct
{
    user_thread_t tcb;
    void (*func)(user_thread_t *t);
} user_thread_entry_t;

// 🔼 Now we have 3 user-level threads
#define NUM_USER_THREADS 3
static user_thread_entry_t g_threads[NUM_USER_THREADS];

// -----------------------------
// Init user-level threads
// -----------------------------

static void init_user_threads(void)
{
    uint64_t now = esp_timer_get_time(); // current time in microseconds

    // UA: shorter period (300 ms)
    g_threads[0].tcb.name = "UA";
    g_threads[0].tcb.id = 0;
    g_threads[0].tcb.period_us = 300000;    // 300 ms
    g_threads[0].tcb.next_release_us = now; // ready to run immediately
    g_threads[0].tcb.last_run_us = 0;
    g_threads[0].tcb.min_delta_us = UINT64_MAX;
    g_threads[0].tcb.max_delta_us = 0;
    g_threads[0].tcb.sum_delta_us = 0;
    g_threads[0].tcb.run_count = 0;
    g_threads[0].tcb.deadline_misses = 0;
    g_threads[0].tcb.worst_lateness_us = 0;
    g_threads[0].func = user_thread_A_body;

    // UB: medium period (500 ms)
    g_threads[1].tcb.name = "UB";
    g_threads[1].tcb.id = 1;
    g_threads[1].tcb.period_us = 500000; // 500 ms
    g_threads[1].tcb.next_release_us = now;
    g_threads[1].tcb.last_run_us = 0;
    g_threads[1].tcb.min_delta_us = UINT64_MAX;
    g_threads[1].tcb.max_delta_us = 0;
    g_threads[1].tcb.sum_delta_us = 0;
    g_threads[1].tcb.run_count = 0;
    g_threads[1].tcb.deadline_misses = 0;
    g_threads[1].tcb.worst_lateness_us = 0;
    g_threads[1].func = user_thread_B_body;

    // UC: longer period (700 ms) 🔹 NEW
    g_threads[2].tcb.name = "UC";
    g_threads[2].tcb.id = 2;
    g_threads[2].tcb.period_us = 700000; // 700 ms
    g_threads[2].tcb.next_release_us = now;
    g_threads[2].tcb.last_run_us = 0;
    g_threads[2].tcb.min_delta_us = UINT64_MAX;
    g_threads[2].tcb.max_delta_us = 0;
    g_threads[2].tcb.sum_delta_us = 0;
    g_threads[2].tcb.run_count = 0;
    g_threads[2].tcb.deadline_misses = 0;
    g_threads[2].tcb.worst_lateness_us = 0;
    g_threads[2].func = user_thread_C_body;
}

// -----------------------------
// Scheduler task (RR or EDF)
// -----------------------------

static void scheduler_task(void *param)
{
    (void)param;

#if SCHED_POLICY == SCHED_POLICY_RR
    printf("Starting user-level scheduler with policy = ROUND ROBIN\n");
#elif SCHED_POLICY == SCHED_POLICY_EDF
    printf("Starting user-level scheduler with policy = EARLIEST DEADLINE FIRST (EDF)\n");
#else
    printf("Starting user-level scheduler with UNKNOWN policy value!\n");
#endif

    init_user_threads();

    int current_index = 0;

    while (1)
    {
        uint64_t now = esp_timer_get_time();
        bool ran_any = false;
        int idx_to_run = -1;

        // -----------------------------
        // Pick which user-thread to run
        // -----------------------------
        if (SCHED_POLICY == SCHED_POLICY_RR)
        {
            // ----- Round Robin -----
            for (int i = 0; i < NUM_USER_THREADS; i++)
            {
                int idx = (current_index + i) % NUM_USER_THREADS;
                user_thread_t *t = &g_threads[idx].tcb;
                if (now >= t->next_release_us)
                {
                    idx_to_run = idx;
                    break;
                }
            }
        }
        else
        {
            // ----- EDF: earliest deadline wins -----
            uint64_t best_deadline = UINT64_MAX;
            for (int i = 0; i < NUM_USER_THREADS; i++)
            {
                user_thread_t *t = &g_threads[i].tcb;
                if (now >= t->next_release_us && t->next_release_us < best_deadline)
                {
                    best_deadline = t->next_release_us;
                    idx_to_run = i;
                }
            }
        }

        if (idx_to_run >= 0)
        {
            user_thread_entry_t *entry = &g_threads[idx_to_run];
            user_thread_t *t = &entry->tcb;

            uint64_t now2 = esp_timer_get_time();
            uint64_t delta = (t->last_run_us == 0)
                                 ? 0
                                 : now2 - t->last_run_us;

            printf("[UThread %s] now=%llu us, delta=%llu us (~%.2f ms)\n",
                   t->name,
                   (unsigned long long)now2,
                   (unsigned long long)delta,
                   (delta == 0 ? 0.0 : (double)delta / 1000.0));

            // Deadline check: if the observed period > desired period,
            // we treat that as a (soft) deadline miss.
            if (t->last_run_us != 0 && delta > t->period_us)
            {
                uint64_t lateness_us = delta - t->period_us;
                t->deadline_misses++;

                if (lateness_us > t->worst_lateness_us)
                {
                    t->worst_lateness_us = lateness_us;
                }

                printf("[Deadline MISS %s] lateness=%llu us (~%.2f ms)\n",
                       t->name,
                       (unsigned long long)lateness_us,
                       (double)lateness_us / 1000.0);
            }

            // Update stats for this user-thread (after the first run)
            if (delta > 0)
            {
                t->run_count++;
                t->sum_delta_us += delta;

                if (delta < t->min_delta_us)
                {
                    t->min_delta_us = delta;
                }
                if (delta > t->max_delta_us)
                {
                    t->max_delta_us = delta;
                }

                // Print a summary every 10 runs
                if (t->run_count % 10 == 0)
                {
                    double avg_ms = (double)t->sum_delta_us /
                                    (double)t->run_count / 1000.0;
                    double min_ms = (double)t->min_delta_us / 1000.0;
                    double max_ms = (double)t->max_delta_us / 1000.0;
                    double worst_late_ms = (double)t->worst_lateness_us / 1000.0;

                    printf("[Stats %s] runs=%lu avg=%.2f ms min=%.2f ms max=%.2f ms "
                           "misses=%lu worst_late=%.2f ms\n",
                           t->name,
                           (unsigned long)t->run_count,
                           avg_ms,
                           min_ms,
                           max_ms,
                           (unsigned long)t->deadline_misses,
                           worst_late_ms);
                }
            }

            t->last_run_us = now2;
            t->next_release_us = now2 + t->period_us;

            // "Run" the user-level thread body
            entry->func(t);

            // For RR, start from the next index next time.
            // For EDF, this doesn't matter much but is harmless.
            current_index = (idx_to_run + 1) % NUM_USER_THREADS;
            ran_any = true;
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

static void user_thread_C_body(user_thread_t *t)
{
    // Simulate another kind of "work"
    volatile int x = 0;
    for (int i = 0; i < 1500; i++)
    {
        x ^= i;
    }
    printf("  -> [UThread %s] did some work (xor loop)\n", t->name);
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
