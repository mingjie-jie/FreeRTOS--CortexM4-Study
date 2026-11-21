# 15｜从头实现 `OS_Delay()`：阻塞、Tick、唤醒、再次恢复 PC

> 接续 [10-MiniRTOS 开发路线](10-MiniRTOS开发路线图.md)。先理解任务状态机和调度条件，再把逻辑接入已验证的真实 SVC/PendSV 移植层。**本章伪代码不是可直接烧入芯片的完整 RTOS**。

## 1. `HAL_Delay` 和 `osDelay` 的本质区别

| 接口 | 典型情况 | 阻塞方式 |
|---|---|---|
| `HAL_Delay(ms)` | STM32 HAL 时基等待 | 函数里轮询 Tick；不会自动将当前 FreeRTOS 任务置为 Blocked |
| `osDelay()` CMSIS-RTOS v1 | 参数通常是毫秒 | 适配层换算为 FreeRTOS Tick，再调用 `vTaskDelay` |
| `osDelay()` CMSIS-RTOS v2 | 参数是 Tick | 适配层调用 `vTaskDelay(ticks)` |
| 自己的 `OS_Delay(ticks)` | 本章设计 | 设置当前任务阻塞状态、唤醒时刻并请求调度 |

如果 `configTICK_RATE_HZ=1000`，则 1 Tick 约为 1ms；若配置为 100，1 Tick 约为 10ms。**不要默认所有 API 的 1000 都等于 1000ms**。

## 2. 要新增哪些数据？

```c
#include <stdint.h>

typedef enum { TASK_READY, TASK_BLOCKED } TaskState;

typedef struct TCB {
    uint32_t *pxTopOfStack;       // 必须与 PendSV 汇编保持相同偏移
    volatile TaskState state;
    volatile uint32_t wakeTick;
    uint8_t priority;
} TCB;

extern TCB * volatile pxCurrentTCB;
extern volatile uint32_t g_tick;
```

这是供**概念解释**的简化结构。真实内核还需要任务链表、空闲任务、临界区/中断管理、溢出处理、启动状态等。尤其在多源中断环境下，`volatile` **不能代替同步**。

## 3. 状态机：调用 OS_Delay 之后发生什么？

```text
Task1 RUNNING
    └─ OS_Delay(1000)
         ├─ 记录 wakeTick = now + 1000
         ├─ 标记 BLOCKED（从 Ready 逻辑移除）
         ├─ 请求 PendSV
         └─ 暂停在 OS_Delay 内的一条指令附近
                   │
                   ▼
            Task2 / Idle 继续运行
                   │
                   ▼
             SysTick 累加 g_tick
                   │
            到期？ → state = READY
                   │
                   ▼
             调度器再次选择 Task1
                   │
                   ▼
          PendSV 通过 PSP 恢复原 PC
                   │
                   ▼
          OS_Delay 正常返回到 Task1
```

任务延时到期只变为 **READY**；是否马上实际 RUNNING，还取决于任务优先级、抢占式调度配置和其他任务状态。

## 4. `OS_Delay` 的核心伪代码

```c
void OS_Delay(uint32_t ticks)
{
    // 限定：只允许在任务上下文调用，调度器已启动
    if (ticks == 0) {
        OS_Yield();             // 主动让出，不进入阻塞
        return;
    }

    OS_EnterCritical();          // 必须使用可靠的嵌套/恢复策略
    pxCurrentTCB->wakeTick = g_tick + ticks;
    pxCurrentTCB->state = TASK_BLOCKED;
    OS_RequestPendSV();         // 只挂起，不直接调用 Handler
    OS_ExitCritical();           // 允许 PendSV 运行

    // 再次执行到这里时，表示本任务已经被重新调度
}
```

必须明确：**在异常发生前，CPU 并不自动跳过 `OS_Delay()` 的返回路径**。被切出的任务恢复后，会先完成延时函数余下的机器指令和函数返回，再执行调用处下一行 C 代码。

### 关于临界区的重要改进

教学示例有时直接写 `__disable_irq(); ...; __enable_irq();`。正式设计应当能保存/恢复进入前的中断屏蔽状态，并正确处理嵌套。如果调用方原本已关闭中断，简单 `__enable_irq()` 会意外开启中断。

## 5. SysTick 如何唤醒任务？

两个任务的最简策略可以遍历 TCB：

```c
void OS_TickISR(void)
{
    ++g_tick;
    bool need_resched = false;

    for (size_t i = 0; i < USER_TASK_COUNT; i++) {
        TCB *t = user_tasks[i];
        if (t->state == TASK_BLOCKED && TickReached(g_tick, t->wakeTick)) {
            t->state = TASK_READY;
            need_resched = true;
        }
    }
    if (need_resched) OS_RequestPendSV();
}
```

这是**示意逻辑**：头文件、`TickReached`、调度器初始化、任务列表和同步接口在自己的实现里补全。真正 FreeRTOS 为了效率使用按唤醒时间组织的延时列表，并处理 Tick 回绕、调度器暂停等情况，不是每次都遍历所有任务。

### 32 位 Tick 回绕

`g_tick` 到达 `0xFFFFFFFF` 后回到 `0`。常用的小型内核做法是限制**单次延时小于 `2^31` Tick**，并使用基于差值的模运算比较。这样必须确保在跨越较长时间前有合理的周期检查，或者建立更严谨的溢出列表策略。

对可移植标准 C 而言，从超范围 `uint32_t` 转成 `int32_t` 的具体结果是实现定义；STM32 ARM 编译器通常采用补码结果。正式跨平台实现应根据编译环境和时间比较策略作明确约束。

## 6. 为什么一定要 Idle 任务？

假设：

```text
Task1: BLOCKED, wakeTick=1000
Task2: BLOCKED, wakeTick=500
```

没有 Idle 时，调度器根本找不到 READY 任务。Idle 应当**始终保持可运行**，成为兜底任务：

```c
void IdleTask(void *arg)
{
    (void)arg;
    for (;;) { __WFI(); } // 若功耗与中断配置允许
}
```

Idle 本身也需要独立栈、TCB 与初始上下文。执行 `__WFI()` 后并不是“CPU 永不再运行”；SysTick 等中断可以将其唤醒。

## 7. 调度器必须跳过 BLOCKED

最简伪代码：

```c
TCB *choose_next(void)
{
    TCB *best = &idle_tcb;       // 永远 READY，最低优先级
    for (size_t i = 0; i < task_count; i++) {
        TCB *t = tasks[i];
        if (t->state != TASK_READY) continue;
        if (t->priority > best->priority) best = t;
    }
    return best;
}
```

这里只说明**状态过滤 + 优先级比较**；同优先级轮转、列表效率、中断并发和抢占条件另行完善。`vTaskSwitchContext()` 修改 `pxCurrentTCB`，PendSV 再根据新 TCB 恢复 PSP 和 PC。

## 8. 正确的验证场景

| 时刻 | Task1 | Task2 | Idle |
|---|---|---|---|
| 刚启动 | READY → RUNNING | READY | READY |
| Task1 延时 500 Tick | BLOCKED | RUNNING | READY |
| Task2 延时 1000 Tick | BLOCKED | BLOCKED | RUNNING |
| Task1 到期 | READY/RUNNING | BLOCKED | READY |
| Task2 到期 | 根据调度选择 | READY/RUNNING | READY |

验证点：独立任务栈不被破坏，局部计数值保持延续，`OS_Delay` 从调用后继续，而非反复进入任务入口。可以使用 [`examples/delay_state_machine`](../examples/delay_state_machine/) 的主机纯 C 模拟理解任务状态（**它不实现真实抢占**）。

## 9. 总结

**RTOS 延时 ≠ 自旋等待**。它是“记录唤醒 Tick → 任务 BLOCKED → 请求 PendSV → Tick 到期转 READY → 被选中后恢复 PC”的组合功能。需要依赖稳定的 PendSV、TCB、任务列表、Idle、SysTick 和临界区。更多可对照官方 `tasks.c` 的 `vTaskDelay()`、`prvAddCurrentTaskToDelayedList()`、`xTaskIncrementTick()`。
