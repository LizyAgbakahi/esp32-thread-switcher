# ESP32 User-Level Scheduler (Round Robin vs Earliest Deadline First)

## Overview

This project implements a user-level scheduling system on top of the ESP32’s FreeRTOS environment. Instead of relying on the built-in FreeRTOS scheduler, the project defines its own “threads,” manages their release times, performs scheduling decisions, and measures real-time timing behavior.

The goal is to explore how OS schedulers operate internally, compare scheduling policies, and analyze timing, jitter, and deadline-miss behavior on real embedded hardware. This fulfills the requirement of building new scheduling functionality *on top of* FreeRTOS.

---

## Features

### User-Level Scheduling Framework
- Custom TCB (thread control block)
- Software-managed:
  - release times  
  - observed periods  
  - deadlines  
  - timing statistics  

### Two Scheduling Policies Implemented
1. Round Robin (RR)  
2. Earliest Deadline First (EDF)

Policy is selected with:

#define SCHED_POLICY SCHED_POLICY_RR
// or
#define SCHED_POLICY SCHED_POLICY_EDF


### Real-Time Instrumentation
For each user thread:
- Minimum, maximum, average period  
- Jitter  
- Deadline misses  
- Worst-case lateness  
- Stats printed every 10 runs  

---

## Architecture

### User-Level Thread Structure

typedef struct {
const char *name;
uint64_t period_us;
uint64_t next_release_us;
uint64_t last_run_us;
int id; 
}

uint64_t min_delta_us;
uint64_t max_delta_us;
uint64_t sum_delta_us;
uint32_t run_count;

uint32_t deadline_misses;
uint64_t worst_lateness_us;


Three threads implemented:
- UA → 300 ms period
- UB → 500 ms period
- UC → 700 ms period
  

### Scheduler Design

The scheduler runs inside one FreeRTOS task:

xTaskCreate(scheduler_task, "scheduler_task", 4096, NULL, 5, NULL);


Loop behavior:
1. Check which thread is ready  
2. Select between RR or EDF  
3. Log timing/stats  
4. Run the user-thread’s body function  
5. Update deadlines and next release  

---

## Scheduling Algorithms

### Round Robin (RR)
- Fair rotation among ready threads  
- Simple  
- Not deadline-aware  

### Earliest Deadline First (EDF)
- Chooses thread with earliest next deadline  
- Dynamically changing priorities  
- Better real-time guarantees  
- Fewer deadline misses  

---

## Results Summary

To evaluate the scheduler, both **two-thread** and **three-thread** configurations were measured under **Round Robin (RR)** and **Earliest Deadline First (EDF)**.

---

### 2 Threads (300 ms & 500 ms)

#### Round Robin (RR)
- UA avg ≈ **300.01 ms**
- UB avg ≈ **500.02 ms**
- Occasional jitter  
- Small deadline misses

#### Earliest Deadline First (EDF)
- UA ≈ **300.00 ms**
- UB ≈ **500.00 ms**
- Very low jitter  
- Fewer misses, more stable timing

➡ **EDF already outperforms RR even with only two periodic threads.**

---

### 3 Threads (300 ms, 500 ms, 700 ms)

To increase load, a third periodic task **(UC)** was added.

#### Round Robin (RR)
RR cycles fairly but ignores deadlines, resulting in significant jitter and frequent misses:

- **UA (300 ms):** avg ≈ 308 ms, misses = 20, worst lateness ≈ 10 ms  
- **UB (500 ms):** avg ≈ 508 ms, misses = 10, worst lateness ≈ 10 ms  
- **UC (700 ms):** avg ≈ 709–710 ms, misses = 10, worst lateness ≈ 10 ms  

➡ **RR fails to meet deadlines when load increases.**

#### Earliest Deadline First (EDF)
EDF always schedules the thread with the earliest deadline:

- **UA (300 ms):** avg ≈ 300.05 ms, extremely low jitter  
- **UB (500 ms):** avg ≈ 500.20 ms, jitter < 5 ms  
- **UC (700 ms):** avg ≈ 709–710 ms  

➡ **EDF maintains stable timing and dramatically reduces deadline misses.**

## Overall Comparison

| Metric              | Round Robin (RR)       | Earliest Deadline First (EDF) |
|---------------------|------------------------|-------------------------------|
| Deadline Awareness  | ❌ No                  | ✅ Yes                        |
| Jitter              | High                   | Low                           |
| UA Timing Accuracy  | Poor (misses often)    | Excellent (very stable)       |
| UB/UC Accuracy      | Moderate               | Good/Very Good                |
| Worst-Case Lateness | ~10 ms                 | Lower and more stable         |


Overall: **EDF performs better for real-time deadlines.**


---

## How to Build & Run

### 1. Set ESP-IDF environment

cd ~/esp/esp-idf
. ./export.sh


### 2. Build project

cd ~/esp/thread_switcher
idf.py build


### 3. Flash

idf.py -p /dev/cu.usbserial-0001 flash


### 4. Monitor output


Exit: `Ctrl + ]`

---

## Repo Structure
thread_switcher/
│
├── main/
│   └── thread_switcher.c
│
├── notes/
│   ├── rr_logs_300_500.txt              # Round Robin, 2 threads (300ms, 500ms)
│   ├── edf_logs_300_500.txt             # EDF, 2 threads (300ms, 500ms)
│   ├── rr_300_500_700.txt               # Round Robin, 3 threads (300ms, 500ms, 700ms)
│   └── edf_300_500_700.txt              # EDF, 3 threads (300ms, 500ms, 700ms)
│
├── CMakeLists.txt
└── sdkconfig

---

## What I Learned

- How user-level schedulers work
- How to implement custom scheduling logic (RR & EDF)
- How to track real-time timing, jitter, and deadline misses
- How FreeRTOS interacts with timing on real hardware
- How embedded systems differ from simulated environments
- How to use esp_timer and FreeRTOS tasks for precise timing

---

## Conclusion

This project demonstrates a complete user-level scheduler on the ESP32, including real-time instrumentation, RR vs EDF comparison, and deadline analysis. It adds meaningful functionality on top of FreeRTOS and provides insight into the internal workings of OS schedulers and real-time performance on embedded hardware.
