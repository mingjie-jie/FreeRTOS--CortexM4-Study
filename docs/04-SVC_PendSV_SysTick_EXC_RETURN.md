# 04｜Cortex-M 异常机制：SVC、PendSV、SysTick、EXC_RETURN

> **新增扩展阅读**：[14-特殊寄存器与中断屏蔽](14-CortexM特殊寄存器与中断屏蔽.md)、[16-全流程](16-从创建任务到延时唤醒完整链路.md)。

## 1. 异常不等于普通函数调用

Cortex-M4 有 Thread Mode（运行普通程序/任务）与 Handler Mode（异常处理）。Thread Mode 可按 CONTROL.SPSEL 选 MSP 或 PSP；Handler Mode 使用 MSP。常用非 MPU FreeRTOS 在任务中用 PSP，中断/异常中用 MSP。

**异常入栈时使用的是被打断代码原本采用的栈**：如果 Task1 用 PSP，那么进入 PendSV 时，硬件先把 Task1 的基本栈帧压到 Task1 的 PSP 栈，再切到 Handler Mode 使用 MSP。

基本异常栈帧，**从低地址到高地址**：

```text
低地址
  R0
  R1
  R2
  R3
  R12
  LR   ← 被打断代码的 LR 原值
  PC   ← 恢复位置
  xPSR
高地址
```

共 8×4 = 32 字节；实际还有可能的对齐填充及 FPU 扩展帧。进入异常后，**Handler 正在使用的 LR 不再是上面“旧任务 LR”，而是 EXC_RETURN 值**。这两个 LR 容易混淆！

## 2. 四者如何分工

| 概念 | 如何发生 | 在这里的 FreeRTOS 移植中承担什么 |
|---|---|---|
| SVC（异常编号 11） | 软件执行 `SVC #0` | 首次启动任务，跳入 `vPortSVCHandler()` |
| PendSV（14） | 设置 ICSR.PENDSVSET 等方式挂起 | 真正的任务上下文切换 |
| SysTick（15） | 内核 SysTick 周期计数到期 | Tick 计数、检查延时是否到期，需要时挂起 PendSV |
| EXC_RETURN | 异常入口硬件装入 Handler LR 的编码 | 控制异常返回后的模式、栈选择、栈帧格式 |

> PendSV 通常设为最低的可配置异常优先级；这样 ISR 可以先完成，之后再切换任务。SysTick 不是每次都必须切换任务：是否需要 PendSV 取决于调度条件。

## 3. SVC 与首次任务启动

启动阶段通过 `prvStartFirstTask()`：重新加载 MSP、清理 CONTROL.FPCA、启用异常、执行 `SVC 0`。`vPortSVCHandler()` 从 `pxCurrentTCB` 获取第一个任务的人工初始栈帧，恢复 R4～R11 / EXC_RETURN，设置 PSP；`BX LR` 通过硬件恢复任务 PC，进入任务函数。

## 4. PendSV 与后续切换

```text
Task1 ──发生调度请求──► PendSV pending
                             │
                             ▼
                      xPortPendSVHandler
                      ├─ 旧任务寄存器 → 旧任务 PSP 栈
                      ├─ 旧 TCB 保存栈顶
                      ├─ vTaskSwitchContext 选择新 TCB
                      ├─ 新任务栈 → 寄存器、PSP
                      └─ BX LR (EXC_RETURN) ──► Task2
```

示例挂起请求：

```c
SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
```

这不是普通 `BL PendSV_Handler`，而是告诉硬件有异常待处理，随后通过向量表自动进入 Handler。

## 5. EXC_RETURN 常见值（Cortex-M4F）

| 编码 | 目标模式 | 栈选择 | 栈帧 |
|---|---|---|---|
| `0xFFFFFFF1` | Handler | MSP | 基本 |
| `0xFFFFFFF9` | Thread | MSP | 基本 |
| `0xFFFFFFFD` | Thread | PSP | 基本 |
| `0xFFFFFFE1` | Handler | MSP | 浮点扩展 |
| `0xFFFFFFE9` | Thread | MSP | 浮点扩展 |
| `0xFFFFFFED` | Thread | PSP | 浮点扩展 |

重要位：**bit 4**：1 为基本帧，0 为浮点扩展帧；**bit 3**：1 返回 Thread；**bit 2**：1 用 PSP。

```asm
TST R14, #0x10
IT EQ
VSTMDBEQ R0!, {S16-S31}   ; 仅当 EXC_RETURN.bit4 == 0
```

FPU 细节：在使用浮点扩展栈帧时，硬件负责异常机制所规定的低位浮点寄存器和 FPSCR 的保存/恢复（可能配合 lazy stacking），FreeRTOS 软件按需保存 S16～S31。

## 6. BASEPRI、PRIMASK、FAULTMASK 区别

- `BASEPRI`：按优先级阈值屏蔽**部分**可配置异常；FreeRTOS 保护调度数据时使用（不能简单理解为“全关中断”）。
- `PRIMASK`：屏蔽具有可配置优先级的异常；`CPSIE I` 清除该屏蔽。
- `FAULTMASK`：屏蔽除 NMI 外的异常；`CPSIE F` 清除该屏蔽。
- `DSB`：数据同步屏障；`ISB`：指令同步屏障。修改控制寄存器后经常出现。

## 7. 为什么不能直接跳任务入口函数？

跳入口只改变执行位置，没有恢复属于目标任务的 PSP、R4～R11、PC 周围的异常栈帧等。即使“先修改 PC 后跳转”，局部变量访问也可能沿用旧任务栈并损坏数据。**异常返回能一次性把目标任务的执行现场还原**，是 Cortex-M 移植的重要机制。

官方参考：

- https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/portable/RVDS/ARM_CM4F/port.c
- https://developer.arm.com/documentation/dui0553/latest/
