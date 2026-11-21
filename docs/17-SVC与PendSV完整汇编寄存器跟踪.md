# 17｜逐条读汇编：SVC Handler 与 PendSV Handler 的寄存器轨迹

> 你在对话中贴的两个核心函数是 **ARM Compiler 5 / RVDS 风格**的 `__asm void`。完整源片段放在 `reference/`；本章不只翻译注释，更说明每条语句**执行前后寄存器是谁、哪块 RAM 被写了、为什么要这么做**。

## 一、先建立地址模型

假设以下变量都在 SRAM，地址纯教学用途：

```text
地址 0x20000020：pxCurrentTCB 这个全局指针变量
                    内容 = 0x20000100（Task1_TCB 地址）
地址 0x20000100：Task1_TCB 的首成员 pxTopOfStack
                    内容 = Task1 保存现场的栈顶
地址 0x20000200：Task2_TCB 的首成员 pxTopOfStack
                    内容 = Task2 保存现场的栈顶
```

用指针的 C 语言层次表示：

```c
TCB_t *pxCurrentTCB;                // 指针变量本身在某个 SRAM 地址
TCB_t *old = pxCurrentTCB;          // old = 指针变量的“值”
uint32_t *saved = old->pxTopOfStack; // 再取 TCB 首成员的“值”
```

**不要搞混 `&pxCurrentTCB`（指针变量的地址）、`pxCurrentTCB`（TCB 地址）、`pxCurrentTCB->pxTopOfStack`（栈顶地址）。**

## 二、SVC：只恢复首个任务，不保存先前裸机 `main()`

原始 ARMCC5 风格汇编（逐句解释）：

```asm
__asm void vPortSVCHandler(void)
{
    PRESERVE8                   ; 汇编器声明保持 8 字节对齐约定

    LDR R3, =pxCurrentTCB      ; R3 = &pxCurrentTCB
    LDR R1, [R3]              ; R1 = pxCurrentTCB = &Task1_TCB
    LDR R0, [R1]              ; R0 = Task1_TCB.pxTopOfStack

    LDMIA R0!, {R4-R11,R14}   ; 从人工栈的低地址起恢复 9 个字
                                ; R0 上移 36 字节，指向人工基本异常帧
                                ; R14 此时恢复成 0xFFFFFFFD 等 EXC_RETURN
    MSR PSP, R0               ; 设置目标任务的 PSP
    ISB                       ; 指令同步屏障
    MOV R0, #0                ; 清掉临时 R0，随后还会由硬件从帧中恢复任务 R0
    MSR BASEPRI, R0           ; 放开 BASEPRI 屏蔽
    BX R14                    ; 异常返回：CPU 从 PSP 基本帧恢复任务 PC 等
}
```

### 为什么 `MOV R0,#0` 不会把 Task1 的入口参数毁掉？

SVC Handler 的**临时物理 R0** 与“人工异常帧中保存的 Task1 初始 R0”是两个时刻的数据。`BX R14` 后，硬件会从 `[PSP+0]` 恢复任务 R0，因此刚刚设置成 0 的 Handler R0 对任务无害。

### 为什么 `LDMIA` 之后不能把 R0 再当成 `pxTopOfStack`？

起始 `R0 = TCB.pxTopOfStack` 指向**软件保存区**，读取 R4～R11/R14 以后 R0 已向高地址移动 36 字节，指向**硬件基本帧**。这正是 `MSR PSP,R0` 所需要的位置。

### 什么是第一次恢复的 PC？

`pxPortInitialiseStack` 把 `pxCode`（创建任务传入的函数入口指针）写进基本帧的 PC 槽；xPSR.T 设置 Thumb 状态；最后异常返回自动从基本帧 `+0x18` 恢复 PC。SVC Handler **根本不用查函数名，也不需要写一条 `BL Task1`**。

## 三、PendSV：既保存旧任务，又恢复新任务

下面逐条按 **写入/读取对象** 对照你提供的 ARMCC 版本：

| 指令 | 执行前主要内容 | 指令结果与目的 |
|---|---|---|
| `MRS R0,PSP` | PSP 指向旧任务硬件异常帧 | R0=旧 PSP；**不改变 PSP** |
| `ISB` | — | 指令同步 |
| `LDR R3,=pxCurrentTCB` | 全局变量地址固定 | R3=`&pxCurrentTCB` |
| `LDR R2,[R3]` | `[R3]=&Task1_TCB` | R2=`&Task1_TCB`（旧 TCB） |
| `TST R14,#0x10` | Handler LR=旧任务 EXC_RETURN | 根据 bit4 检测扩展浮点帧 |
| `IT EQ` / `VSTMDBEQ R0!,{S16-S31}` | Bit4=0 时 | 软件保存 16 个高位浮点寄存器（64B） |
| `STMDB R0!,{R4-R11,R14}` | R0=旧任务当前保存位置 | R0 减 36B；存软件核心上下文 |
| `STR R0,[R2]` | R2=旧 TCB 地址 | 旧 TCB 首成员更新为保存后的 R0 |
| `STMDB SP!,{R0,R3}` | Handler SP 是 MSP | 临时保存 R0/R3，保持调用 C 时 8B 栈对齐 |
| `MOV R0,#...` `MSR BASEPRI,R0` | — | 设置内核临界区中断阈值 |
| `DSB` / `ISB` | — | 同步屏障 |
| `BL vTaskSwitchContext` | `pxCurrentTCB=&Task1_TCB` | C 调度器可改为 `&Task2_TCB` |
| `MOV R0,#0` / `MSR BASEPRI,R0` | — | 解除此段 BASEPRI 屏蔽 |
| `LDMIA SP!,{R0,R3}` | MSP 保存了两字 | 取回 Handler 原 R3（全局变量地址） |
| `LDR R1,[R3]` | `[R3]` 已由调度器更新 | R1=`&Task2_TCB` |
| `LDR R0,[R1]` | 新 TCB 首成员保存栈顶 | R0=Task2 保存现场的低地址 |
| `LDMIA R0!,{R4-R11,R14}` | R0=新任务保存区 | 恢复新任务软件现场；R0 上移 36B |
| `TST R14,#0x10` `IT EQ` `VLDMIAEQ ...` | 新 R14=新任务 EXC_RETURN | 按需恢复 S16～S31 |
| `MSR PSP,R0` | R0 指向新任务硬件异常帧 | PSP 指向新任务硬件帧 |
| `ISB` | — | 同步执行状态 |
| `BX R14` | R14=新任务 EXC_RETURN | 异常返回，硬件恢复新任务 R0～R3、原 LR、PC、xPSR 等 |

`WORKAROUND_PMU_CM001` 是特定芯片勘误分支；**STM32F407 的普通移植并不需要开启该 XMC4000 专用分支**。

## 四、R2/R3/R0 究竟各干了什么？时间轴

```text
T0  刚进入 Handler：
    旧 Task1 的 R2 在 PSP 硬件帧保存；Handler 物理 R2 是临时寄存器

T1  LDR R3,=pxCurrentTCB
    R3 = 0x20000020

T2  LDR R2,[R3]
    R2 = 0x20000100 = Task1_TCB 地址

T3  STR R0,[R2]
    Task1_TCB 的 pxTopOfStack 更新完成
    R2 临时任务彻底结束，不再需要它

T4  STMDb SP!,{R0,R3}
    R3（全局变量位置）进入 MSP；准备跨 C 函数调用

T5  BL vTaskSwitchContext
    C 调度器把 [0x20000020] 修改成 0x20000200（Task2_TCB）
    函数可以修改 Handler 当前 R0-R3，包括 R2

T6  LDMIA SP!,{R0,R3}
    重新取回 R3 = 0x20000020

T7  LDR R1,[R3]
    R1 = 0x20000200（Task2_TCB）

T8  LDR R0,[R1]
    R0 = Task2 保存的栈顶

T9  LDMIA ...; MSR PSP,R0; BX R14
    逐步恢复 Task2 原执行现场
```

如果你只记住**一句话**：`R2` 临时放**旧 TCB**，写完栈顶就不再需要；`R3` 放**全局当前 TCB 指针变量的地址**，调度后还要用它重新读取新 TCB。**任务原 R2 早已被硬件压栈，完全不依赖这个临时 R2。**

## 五、最容易犯的六个错误

1. 认为旧任务的 PSP 栈和 SVC/PendSV Handler 的 MSP 栈是同一块。
2. 认为 `STMDB R0!` 直接修改 PSP。
3. 认为 `LDR R3,=pxCurrentTCB` 取出的是 TCB 地址而非**指针变量的地址**。
4. 认为 `LDMIA` 根据变量名智能匹配 R4 / R5；其实它按硬件寄存器列表顺序取固定槽位。
5. 认为 `BL vTaskSwitchContext` 一定导致不同任务运行；调度器可能选回原任务。
6. 认为 SVC Handler `BX R14` 之后返回 `prvStartFirstTask` 里的 NOP；正常情况下它异常返回到人工栈帧中的**任务入口 PC**。

配套练习：[13-内存地址图](13-寄存器现场与异常栈帧详图.md) 和 [18-练习题](18-寄存器与栈计算练习题.md)。
