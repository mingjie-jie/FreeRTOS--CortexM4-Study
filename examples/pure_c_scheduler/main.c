/*
 * 教学实验 A：完全标准 C 的周期性任务调度器
 * 注意：这不是抢占式 RTOS，没有独立任务栈，没有保存 CPU PC/PSP。
 * 每个任务只运行一小段工作，然后返回到 scheduler_step()。
 */
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

typedef void (*TaskFn)(uint32_t now_ms);

typedef struct {
    const char *name;
    TaskFn run;
    uint32_t period_ms;
    uint32_t last_run_ms;
    unsigned priority;
    unsigned calls;
} TaskTCB;

static int led1 = 0;
static int led2 = 0;

static void task1(uint32_t now_ms)
{
    led1 = !led1;
    printf("%4" PRIu32 " ms | Task1 | LED1=%d\n", now_ms, led1);
}

static void task2(uint32_t now_ms)
{
    led2 = !led2;
    printf("%4" PRIu32 " ms | Task2 | LED2=%d\n", now_ms, led2);
}

/* 模拟创建好的两个任务；真实 RTOS 的 TCB 更复杂。 */
static TaskTCB tasks[] = {
    {"Task1", task1, 500u, 0u, 1u, 0u},
    {"Task2", task2, 1000u, 0u, 2u, 0u}
};

#define TASK_COUNT (sizeof(tasks) / sizeof(tasks[0]))

/* 在指定时间点挑选一个到期任务：若多个到期，先执行优先级高的。 */
static TaskTCB *select_due_task(uint32_t now_ms)
{
    TaskTCB *chosen = NULL;
    for (size_t i = 0; i < TASK_COUNT; ++i) {
        TaskTCB *t = &tasks[i];
        if ((uint32_t)(now_ms - t->last_run_ms) < t->period_ms) {
            continue;
        }
        if (chosen == NULL || t->priority > chosen->priority) {
            chosen = t;
        }
    }
    return chosen;
}

/* 协作式：短任务返回后，才轮到调度器寻找下一个任务。 */
static void scheduler_step(uint32_t now_ms)
{
    TaskTCB *t;
    while ((t = select_due_task(now_ms)) != NULL) {
        t->last_run_ms = now_ms;
        ++t->calls;
        t->run(now_ms);     /* 编译器生成函数调用，不是上下文切换 */
    }
}

int main(void)
{
    puts("=== Pure C periodic scheduler (NOT real RTOS switching) ===");
    for (uint32_t now = 0; now <= 2500u; now += 100u) {
        scheduler_step(now);
    }
    printf("calls: Task1=%u, Task2=%u\n", tasks[0].calls, tasks[1].calls);
    assert(tasks[0].calls == 5u);
    assert(tasks[1].calls == 2u);
    puts("PASS: two periodic callbacks were scheduled.");
    return 0;
}
