# 你提供的 ARMCC5 `xPortPendSVHandler()` 原始代码

> 这是你在对话里给出的 Keil ARM Compiler / RVDS 风格汇编片段，适用于**对照完整 FreeRTOS 移植工程学习**；不能单独在电脑 C 编译器上编译，也不应绕过 FreeRTOS 直接替换系统异常处理函数。
>
> 完整逐句解释见 [05-PendSV逐行解析](../docs/05-PendSV逐行解析.md)。以下保留原始指令顺序，方便你和工程一行一行对照。

```c
__asm void xPortPendSVHandler( void )
{
    extern uxCriticalNesting;
    extern pxCurrentTCB;
    extern vTaskSwitchContext;

    PRESERVE8

    mrs r0, psp
    isb
    /* Get the location of the current TCB. */
    ldr r3, =pxCurrentTCB
    ldr r2, [r3]

    /* Is the task using the FPU context?  If so, push high vfp registers. */
    tst r14, #0x10
    it eq
    vstmdbeq r0!, {s16-s31}

    /* Save the core registers. */
    stmdb r0!, {r4-r11, r14}

    /* Save the new top of stack into the first member of the TCB. */
    str r0, [r2]

    stmdb sp!, {r0, r3}
    mov r0, #configMAX_SYSCALL_INTERRUPT_PRIORITY
    msr basepri, r0
    dsb
    isb
    bl vTaskSwitchContext
    mov r0, #0
    msr basepri, r0
    ldmia sp!, {r0, r3}

    /* The first item in pxCurrentTCB is the task top of stack. */
    ldr r1, [r3]
    ldr r0, [r1]

    /* Pop the core registers. */
    ldmia r0!, {r4-r11, r14}

    /* Is the task using the FPU context?  If so, pop the high vfp registers too. */
    tst r14, #0x10
    it eq
    vldmiaeq r0!, {s16-s31}

    msr psp, r0
    isb
    #ifdef WORKAROUND_PMU_CM001 /* XMC4000 specific errata */
        #if WORKAROUND_PMU_CM001 == 1
            push { r14 }
            pop { pc }
            nop
        #endif
    #endif

    bx r14
}
```

## 行号式读法（对应上方顺序）

| 关键指令 | 从哪里获取 | 写到哪里 | 任务阶段 |
|---|---|---|---|
| `MRS R0,PSP` | PSP | R0 | 旧任务 |
| `LDR R3,=pxCurrentTCB` | 符号地址 | R3 | 旧任务 |
| `LDR R2,[R3]` | 指针变量中的值 | R2（旧 TCB 地址） | 旧任务 |
| `VSTMDBEQ R0!,{S16-S31}` | FPU 上半寄存器 | 旧任务栈 | 可选 |
| `STMDB R0!,{R4-R11,R14}` | CPU 寄存器 | 旧任务栈 | 旧任务 |
| `STR R0,[R2]` | R0 保存后的栈顶 | 旧 TCB 首成员 | 旧任务 |
| `STMDB SP!,{R0,R3}` | Handler 工作寄存器 | MSP 栈 | 切换中 |
| `BL vTaskSwitchContext` | 就绪列表 | `pxCurrentTCB` 新值 | 调度器 |
| `LDMIA SP!,{R0,R3}` | MSP 栈 | Handler 工作寄存器 | 切换中 |
| `LDR R1,[R3]` | 更新后的全局指针 | R1（新 TCB） | 新任务 |
| `LDR R0,[R1]` | 新 TCB 首成员 | R0（新栈顶） | 新任务 |
| `LDMIA R0!,{R4-R11,R14}` | 新任务栈 | CPU 寄存器 | 新任务 |
| `MSR PSP,R0` | R0 | PSP | 新任务 |
| `BX R14` | EXC_RETURN | 触发硬件异常返回 | 新任务 |

**重点复习**：R2 原值（被中断任务现场）由异常硬件保存在 PSP 栈；R2 的 Handler 临时值（OldTCB 地址）用完就可以丢，不需要越过 `BL` 保存。R3 被保存是因为调度函数可能更改 R3，但我们仍需要通过它找到全局指针变量。
