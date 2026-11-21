# 05｜逐行分析 xPortPendSVHandler（重点）

> **v2 深化阅读**：想逐条对应 R0/R1/R2/R3 的生命周期，请看 [17-SVC/PendSV 寄存器时间轴](17-SVC与PendSV完整汇编寄存器跟踪.md)；想看真实 SRAM 地址布局，请看 [13-完整栈帧图](13-寄存器现场与异常栈帧详图.md)。

对应你提供的 **Keil / ARM Compiler 5（RVDS/ARMCC 风格）** 代码，原始全文见 [reference](../reference/ARMCC5-PendSV原始代码与注释.md)。以下是分段的真正目的，**不是直接运行的独立完整 Handler**。

## 整体概念：谁在调用谁？

```text
Tick/阻塞/ISR/主动 yield → 请求 PendSV → Cortex-M 按向量表进入 xPortPendSVHandler
保存当前任务栈 → BL vTaskSwitchContext（在 tasks.c 选下一个 TCB）
恢复目标任务栈 → BX EXC_RETURN → CPU 恢复新任务 PC
```

Handler 进入时，旧任务的 R0-R3、R12、原 LR、PC、xPSR **已经**由硬件保存在 PSP 栈。Handler 的 R0-R3 可以用作临时工作寄存器。

## A. 读取 PSP 与旧任务 TCB

```asm
MRS R0, PSP             ; R0 = PSP，指向基本/扩展硬件异常帧
ISB
LDR R3, =pxCurrentTCB  ; R3 = &pxCurrentTCB
LDR R2, [R3]           ; R2 = pxCurrentTCB = &OldTCB
```

特别注意 `=` 和 `[]`：`LDR R3, =symbol` 是加载**符号地址**的伪指令；`LDR R2, [R3]` 是解引用一次。

## B. 保存软件现场

```asm
TST R14, #0x10         ; 测试 Handler LR 的 EXC_RETURN bit4
IT EQ
VSTMDBEQ R0!, {S16-S31} ; bit4=0 才保存高位浮点寄存器
STMDB R0!, {R4-R11,R14} ; 低地址方向扩展 9 个 32 位槽
STR R0, [R2]           ; OldTCB->pxTopOfStack = R0
```

`STMDB R0!, {...}` **操作 R0 的数值**，不直接写 PSP；`!` 使 R0 变成新的保存现场栈顶。R14 这里保存的是 **EXC_RETURN**，不是 Task1 原来的函数返回 LR。后者已经在硬件栈帧中。

## C. 保护 Handler 现场并调用 C 调度器

```asm
STMDB SP!, {R0,R3}     ; Handler Mode 的 SP 是 MSP，暂存 R0/R3
MOV R0, #configMAX_SYSCALL_INTERRUPT_PRIORITY
MSR BASEPRI, R0         ; 暂时屏蔽允许调用内核 API 的中断范围
DSB
ISB
BL vTaskSwitchContext   ; 由调度器更新 pxCurrentTCB
MOV R0, #0
MSR BASEPRI, R0         ; 清除屏蔽阈值
LDMIA SP!, {R0,R3}     ; 恢复 Handler 的 R0/R3
```

- **为什么不保存 R2？** 旧任务栈顶已写入 OldTCB。R2 的临时用途结束；`BL` 后可以变成别的值。被切出任务的原 R2 在硬件异常栈帧中，没有丢失。
- **为什么偏偏保存 R3？** R3 = 全局变量 `pxCurrentTCB` 自身的地址；即使调度器把它的值改为 `&NewTCB`，R3 指向的 RAM 位置不变。且 `BL` 调用的 C 函数可以覆写 R3，必须临时保护。
- **为什么顺带保存 R0？** 两个寄存器共 8 字节，维持调用 C 函数时的栈对齐，R0 旧值稍后会被覆盖；没必要保留 R2。
- `PRESERVE8` 是 ARM 汇编器的栈对齐声明，并不是单靠一个标记就能修复错误的栈布局。
- 这里用 `BASEPRI` 而不直接全关中断，服务更高优先级、不能调用 RTOS API 的中断依然有机会响应。

## D. 读取新任务 TCB，恢复软件现场

```asm
LDR R1, [R3]            ; R1 = pxCurrentTCB = &NewTCB
LDR R0, [R1]            ; R0 = NewTCB->pxTopOfStack
LDMIA R0!, {R4-R11,R14} ; 恢复 R4-R11 与该任务的 EXC_RETURN
TST R14, #0x10
IT EQ
VLDMIAEQ R0!, {S16-S31} ; 按需恢复 FPU 高位寄存器
MSR PSP, R0             ; PSP 指向新任务的硬件异常栈帧
ISB
BX R14                 ; 特殊异常返回：硬件恢复新任务 PC 等
```

**为什么 R1 不直接用 R2？** R2 只是旧 TCB 地址，而且没有跨越调度器调用保留；新的 `pxCurrentTCB` 值必须在 `vTaskSwitchContext()` 返回后再次解引用。

## E. 一份具体内存快照

假设：

```text
&pxCurrentTCB = 0x20000020
进入 PendSV 前： [0x20000020] = 0x20000100（Task1_TCB）
R3 = 0x20000020
R2 = 0x20000100
R0 = 保存软件寄存器后的旧任务栈顶 = 0x20000FDC
STR R0, [R2] → [0x20000100] = 0x20000FDC

调度器修改：  [0x20000020] = 0x20000200（Task2_TCB）
LDR R1, [R3] → R1 = 0x20000200
LDR R0, [R1] → R0 = Task2 保存好的栈顶
LDMIA ... + MSR PSP + BX LR → 回到 Task2
```

地址都只是教学示意：不要把它们硬编码到工程中。

## F. 经常出现的误解

1. **R2 没有保存，R2 的任务内容丢了？** 不会。任务原 R2 是硬件保存的；Handler 的临时 R2 不需要保存。
2. **PendSV 直接跳到 Task2 的函数入口？** 不是；是恢复 Task2 上次停止位置的 PC，第一次运行则使用人工初始栈帧中的入口 PC。
3. **`STMDB R0!` 更新了 PSP？** 没有，更新的是 R0；随后写入 TCB。恢复另一任务时才 `MSR PSP,R0`。
4. **`BL vTaskSwitchContext` 一定换任务？** 不一定；调度器也可以选回相同任务。
5. **`BX R14` 和普通 `BX LR` 一样？** 字面指令一样，但 LR 中若是 EXC_RETURN，硬件执行异常返回操作。
6. **`WORKAROUND_PMU_CM001` 是 STM32F407 必须代码？** 不是，它是 XMC4000 特定勘误处理分支。

下一篇：[首次任务启动](06-首次任务启动与初始栈帧.md)；上一章：[TCB / PSP / MSP](03-TCB_PSP_MSP与任务栈.md)。
