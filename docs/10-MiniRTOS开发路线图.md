# 10｜自己做双任务 Mini RTOS：从纯 C 到真实 PendSV

> 延时实现学习：[15-自己做 OS_Delay](15-osDelay阻塞唤醒与系统Tick.md)；整体流程：[16-任务创建到延时唤醒](16-从创建任务到延时唤醒完整链路.md)。

> 这个章节是**未来动手计划**，不是声称仓库中已实现完整的 Cortex-M4F 可运行内核。当前可运行示例是纯 C 调度与上下文内存模拟。真正的抢占式实现需根据目标芯片、工具链、FPU 和异常向量表编写、测试移植层。

## 阶段 A：纯 C 协作式调度

实现 `task_create()`（简化 TCB）、`scheduler_select_next()`、`scheduler_run()`；任务每次执行有限工作后返回。示例见 `examples/pure_c_scheduler`。

**通过条件**：不同周期任务可以执行；能观察调度策略；但不具备独立栈/任意位置恢复。

## 阶段 B：真实两个独立任务栈，手动 yield

建议模块/函数及依赖顺序：

| 次序 | 接口 | 语言 | 功能 |
|---:|---|---|---|
| 1 | `task_stack_init()` | C | 创建符合 ARM 预期的初始异常栈帧 |
| 2 | `task_create()` | C | 初始化 TCB 与栈，关联任务函数/参数 |
| 3 | `scheduler_init()` | C | 初始化调度状态与任务列表 |
| 4 | `scheduler_select_next()` | C | 只在两个就绪任务之间轮流选取 |
| 5 | `scheduler_start()` | C + 移植层 | 设置异常优先级并启动第一个任务 |
| 6 | `SVC_Handler()` | 汇编 | 恢复首个任务上下文与 PSP，异常返回 |
| 7 | `task_yield()` | C + CMSIS | 设置 PendSV pending |
| 8 | `PendSV_Handler()` | 汇编 | 保存/恢复上下文，调用 C 调度器 |

初始 TCB 结构必须至少包含（用教学简化结构）：

```c
typedef struct TCB {
    uint32_t *pxTopOfStack;  // 必须与汇编访问方式一致：偏移为 0
    uint32_t *stack_base;
    void (*entry)(void *arg);
    void *arg;
} TCB;
```

**必须验证**：

- 第一个任务通过 SVC 恢复人工初始栈帧并运行。
- `task_yield()` 可以让两个 `while(1)` 任务交替执行。
- 两个任务的自动局部计数变量持续分别增长，而不是每次重新初始化。
- PSP 在两个不同的 RAM 栈区间变化。
- PendSV 只保存/恢复软件现场；异常硬件自动栈帧不被破坏。

## 阶段 C：SysTick 抢占调度

添加 `task_tick()`、`SysTick_Handler()` 与时间片轮转。正确处理 `HAL_IncTick()` 与 RTOS Tick 的关系，避免两个组件抢占同一个中断 Handler。**PendSV** 要设为最低可配置异常优先级。需要稳定地完成中断优先级和 8 字节栈对齐。

## 阶段 D：阻塞、唤醒和优先级

添加 `task_delay(ticks)`、就绪列表、阻塞列表和到期任务重返 READY；不是到期后强制跳函数入口，而是再次选中任务后从保存的 PC 恢复。实现临界区、中断中 API 的优先级规则与 `BASEPRI` 屏蔽。

## 阶段 E：面向 Cortex-M4F 的健壮性

- FPU 高位寄存器 S16～S31 与 EXC_RETURN.bit4；硬件 FPU 扩展栈帧与 lazy stacking。
- `configMAX_SYSCALL_INTERRUPT_PRIORITY` 与中断优先级位数、分组的对应关系。
- MSP / 任务栈溢出检查、栈水位/哨兵。
- 优先级抢占、空闲任务、同步机制（通知、信号量、队列）。
- 异常向量表 Handler 名称映射（Keil ARMCC5 与 GCC 语法不同）。

## 安全边界

`SVC_Handler` / `PendSV_Handler` 一旦错误可能直接 HardFault，或者出现难以复现的寄存器损坏。初次实现请在可调试板卡、保留故障处理和串口日志的环境中进行；不要直接替换正在工作的生产固件移植层。完成每个阶段的 PC/PSP/TCB 检查后再扩展下一功能。
