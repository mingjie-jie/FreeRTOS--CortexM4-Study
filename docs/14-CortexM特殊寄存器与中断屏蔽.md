# 14｜特殊寄存器全景：xPSR、CONTROL、MSP/PSP、BASEPRI、VTOR、ICSR、FPU

> 阅读 `prvStartFirstTask`、`vPortSVCHandler`、`xPortPendSVHandler` 必备。一般通用寄存器 R0～R15 见 [02](02-寄存器与局部变量.md)，本章聚焦**“CPU 当前是什么状态、硬件为什么能切换任务”**。

## 1. 一页速查表

| 寄存器 / 模块 | 典型访问方式 | 用处 | 在你贴过的 FreeRTOS 源码中 |
|---|---|---|---|
| `MSP` | `MRS/MSR MSP` | 主栈指针，异常处理使用 | `prvStartFirstTask` 重置 MSP |
| `PSP` | `MRS/MSR PSP` | 进程栈指针，任务线程通常使用 | PendSV 读旧 PSP、写新 PSP |
| `CONTROL` | `MRS/MSR CONTROL` | Thread 权限、Thread 选栈、FPCA | 首次启动清 CONTROL，为 SVC 做准备 |
| `xPSR` | 多数调试器可直接查看 | 状态标志、Thumb 状态、异常编号 | 硬件异常栈帧最后一个主要字 |
| `PRIMASK` | `CPSID I/CPSIE I` | 屏蔽可配置优先级异常 | 首次启动 `CPSIE I` |
| `BASEPRI` | `MSR BASEPRI,R0` | 按优先级阈值屏蔽一部分可配置异常 | PendSV 保护内核调度过程 |
| `FAULTMASK` | `CPSID F/CPSIE F` | 屏蔽除 NMI 外的异常 | 首次启动 `CPSIE F` |
| `SCB->VTOR` | `*(uint32_t*)0xE000ED08` | 当前异常向量表基地址 | 启动代码通过 VTOR 读取初始 MSP |
| `SCB->ICSR` | `SCB->ICSR` | 异常状态、PendSV 设置/清除等 | `PENDSVSET` 请求任务切换 |
| `SCB->CCR` | `SCB->CCR` | 一些栈、对齐和异常控制配置 | 影响 STKALIGN 等行为 |
| `FPCCR/FPSCR` | CMSIS/寄存器接口 | 浮点上下文延迟保存与状态 | 影响含 FPU 的栈帧处理 |

## 2. CONTROL：为什么不能“直接改 PC 就实现任务切换”？

Cortex-M4 的 CONTROL 常见位：

| 位 | 名称 | 0 | 1 |
|---|---|---|---|
| 0 | nPRIV | Thread 特权级 | Thread 非特权级 |
| 1 | SPSEL | Thread 使用 MSP | Thread 使用 PSP |
| 2 | FPCA（有 FPU 的内核） | 无活动浮点上下文标记 | 有活动浮点上下文标记 |

Handler Mode **始终使用 MSP**。在标准 FreeRTOS Cortex-M 非 MPU 场景中，任务通常 Thread Mode + PSP，异常 Handler Mode + MSP。

你贴的 `prvStartFirstTask` 中：

```asm
MOV R0, #0
MSR CONTROL, R0
```

这在首次 SVC 之前清理了 FPCA，并使 Thread 状态处于特权、MSP 选择等设定。随后 SVC Handler 恢复首个任务的 PSP，最后 `BX EXC_RETURN` 令 CPU 返回 Thread + PSP。**CONTROL 不是“保存 PC 的寄存器”**。

## 3. xPSR：CPU 的执行状态

`xPSR` 可以理解为三个程序状态视图的组合：

- APSR：算术/逻辑条件标志，如 N、Z、C、V 等。
- IPSR：当前异常编号（Thread 模式通常为 0）。
- EPSR：执行状态，其中 Thumb 状态 T 位很重要。

在初始任务栈帧中，经常看到 `0x01000000`：设置了 xPSR 的 T 位（bit24）。Cortex-M 只能执行 Thumb 指令，构造初始异常栈帧时必须保证合法执行状态。

异常基本帧里保存 xPSR，故抢占后恢复的不仅是 PC，条件标志等执行状态也能恢复。**这也是只改 PC 不够的原因之一**。

## 4. PRIMASK 与 BASEPRI：为什么 FreeRTOS 不随便全关中断？

### PRIMASK

```asm
CPSID I    ; 将 PRIMASK 置为屏蔽状态（常用全局关普通中断）
CPSIE I    ; 清 PRIMASK
```

它影响具备可配置优先级的异常，而不能屏蔽 NMI/HardFault 这类更高优先级异常。

### BASEPRI

```asm
MOV R0, #configMAX_SYSCALL_INTERRUPT_PRIORITY
MSR BASEPRI,R0
DSB
ISB
BL vTaskSwitchContext
MOV R0,#0
MSR BASEPRI,R0
```

`BASEPRI` 表示一个屏蔽阈值。达到/低于紧急程度阈值的可配置异常被屏蔽，较高紧急程度的中断仍可运行。**Cortex-M 的 NVIC 数字通常越小表示紧急程度越高**（不是任务优先级！）。硬件优先级字段通常只实现高若干位，移植中需要确保 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 与移位编码一致。

与 FreeRTOS API 直接相关：**高于（紧急程度更高）BASEPRI 阈值、仍可运行的 ISR，不得调用需要 FreeRTOS 内核临界区保护的 ISR API**。否则会破坏内核同步假设。

### 为什么不能随手使用 `__enable_irq()`？

自己写 Mini RTOS 时，进入临界区前必须考虑原 PRIMASK/BASEPRI 状态及嵌套关系。简单 `__disable_irq(); ...; __enable_irq();` 在调用方本来已关中断时，会**错误地提前打开中断**。教学版本应限制调用上下文；正式实现应保存旧屏蔽状态并在退出时恢复，或者使用正确的嵌套临界区接口。

## 5. FAULTMASK：`CPSIE F` 的 F 不是浮点

`CPSIE F` 清 FAULTMASK，解除相应异常屏蔽；它和 FPU 是否开启**没有直接关系**。不要把启动代码中的 `CPSIE F` 理解成“开启浮点”。

## 6. VTOR：中断向量表地址怎么找到？

你贴的 `prvStartFirstTask`：

```asm
LDR R0, =0xE000ED08  ; &SCB->VTOR
LDR R0, [R0]         ; R0 = 当前向量表基址
LDR R0, [R0]         ; R0 = 向量表第0项 = 初始 MSP
MSR MSP, R0
```

向量表索引示意：

```text
VTOR+0x00: 初始 MSP
VTOR+0x04: Reset_Handler 入口
VTOR+0x08: NMI_Handler 入口
VTOR+0x0C: HardFault_Handler 入口
...
VTOR+0x2C: SVC_Handler（异常编号11）
VTOR+0x38: PendSV_Handler（异常编号14）
VTOR+0x3C: SysTick_Handler（异常编号15）
```

这三个异常偏移来自 **异常编号 × 4 字节**。如果 Bootloader 已重定位 VTOR，则不能无脑假设 `VTOR=0x08000000`。

## 7. ICSR：请求 PendSV，而非直接调用 `PendSV_Handler()`

```c
SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
```

本质是请求挂起 PendSV，等待 CPU 按中断优先级和异常机制在合适时机响应。`PENDSVSET` 是 ICSR 的 bit28，`PENDSVCLR` 是 bit27。

进入异常 Handler 并不是普通 `BL PendSV_Handler`：CPU 会自动构造异常栈帧并将 EXC_RETURN 放进 Handler 的 LR。因此**绝不能把 PendSV 当普通函数随便调用**。

## 8. EXC_RETURN 由谁产生？

Cortex-M 硬件进入异常时向 Handler 的 LR 放入特殊编码。常见 `0xFFFFFFFD`：

- bit2=1：返回时使用 PSP；
- bit3=1：返回 Thread Mode；
- bit4=1：基本异常栈帧。

如果 `bit4=0` 则涉及浮点扩展栈帧（Cortex-M4F）。`BX LR` 在 Handler 中遇到有效 EXC_RETURN，将触发**异常返回过程**而不是普通的 PC 地址分支。详见 [04-异常机制](04-SVC_PendSV_SysTick_EXC_RETURN.md)。

## 9. FPU：S0～S31、FPSCR、CONTROL.FPCA 与 lazy stacking

- S0～S31 是 32 个单精度浮点寄存器，每个 32 位。
- 使用浮点扩展异常帧时，低位 S0～S15 和 FPSCR 等由硬件异常机制处理，可能先预留空间、实际写入延迟（lazy stacking）。
- FreeRTOS ARM_CM4F PendSV 根据 EXC_RETURN.bit4 按需 `VSTMDBEQ R0!,{S16-S31}`，恢复时 `VLDMIAEQ`。
- `FPCCR` 包含影响浮点自动保存、延迟堆栈行为的控制位；首次启动代码清 FPCA 以避免不必要的浮点扩展上下文。

重要区分：**FPU 的物理寄存器同样只有一套，任务切换依然要保证不同任务的浮点计算现场互不覆盖。**

## 10. 高频排错清单

1. 未正确配置 SVC/PendSV/SysTick 向量表 → 异常进入错误 Handler。
2. 不理解 Handler 用 MSP，误把 `STMDB SP!` 当作 Task1 PSP 栈操作。
3. NVIC 优先级值没按已实现的优先级位编码 → 临界区掩码失效。
4. `BX LR` 时 LR 已不是 EXC_RETURN → HardFault/INVPC。
5. 栈顶未 8 字节对齐，或 SVC 人工栈帧构造错误 → 异常返回失败。
6. `MSR CONTROL,0` 误认为是永久关闭 FPU。
7. SVC 栈重置后还试图正常返回 `main` 调用链 → 启动流程被设计成不返回。

官方参考：[ARM Cortex-M4 Generic User Guide](https://documentation-service.arm.com/static/5f2ac4ab60a93e65927bbdbf)、[ARM AN298 FPU 延迟入栈说明](https://documentation-service.arm.com/static/5f0352c0cafe527e86f5bc66)。
