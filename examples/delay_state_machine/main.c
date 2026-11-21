/* Pure C teaching simulation for READY/BLOCKED/IDLE. NOT preemptive RTOS. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

typedef enum { READY, BLOCKED } State;
typedef struct {
    const char *name;
    State state;
    uint32_t wake_tick;
    unsigned priority;
    unsigned runs;
} Task;

static Task task1 = {"Task1", READY, 0, 2, 0};
static Task task2 = {"Task2", READY, 0, 1, 0};
static unsigned idle_runs = 0;
static uint32_t now_tick = 0;

/* Only valid for deadlines < 2^31 ticks in the future on 2's-complement host. */
static bool tick_reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

static void wake_if_due(Task *task)
{
    if (task->state == BLOCKED && tick_reached(now_tick, task->wake_tick)) {
        task->state = READY;
        printf("tick=%" PRIu32 ": wake %s\n", now_tick, task->name);
    }
}

static void run_task(Task *task)
{
    ++task->runs;
    printf("tick=%" PRIu32 ": run %s (iteration=%u)\n",
           now_tick, task->name, task->runs);
    task->wake_tick = now_tick + (task == &task1 ? 3u : 5u);
    task->state = BLOCKED;
}

int main(void)
{
    for (now_tick = 0; now_tick < 16; ++now_tick) {
        wake_if_due(&task1);
        wake_if_due(&task2);
        if (task1.state == READY) run_task(&task1);
        else if (task2.state == READY) run_task(&task2);
        else { ++idle_runs; printf("tick=%" PRIu32 ": Idle\n", now_tick); }
    }
    assert(task1.runs >= 3);
    assert(task2.runs >= 2);
    assert(idle_runs > 0);
    puts("PASS: blocked tasks wake and idle runs when both tasks are blocked.");
    return 0;
}
