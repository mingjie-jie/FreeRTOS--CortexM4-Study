# 你提供的 ARMCC5 `prvStartFirstTask()` 原始代码

这是 **首次任务启动** 的移植层汇编，不是每次任务切换都执行。与 PendSV 的职责完全不同。具体分析见 [06-首次任务启动与初始栈帧](../docs/06-首次任务启动与初始栈帧.md)。

```c
__asm void prvStartFirstTask( void )
{
    PRESERVE8

    /* Use the NVIC offset register to locate the stack. */
    ldr r0, =0xE000ED08
    ldr r0, [r0]
    ldr r0, [r0]
    /* Set the msp back to the start of the stack. */
    msr msp, r0
    /* Clear the bit that indicates the FPU is in use in case the FPU was used
    before the scheduler was started - which would otherwise result in the
    unnecessary leaving of space in the SVC stack for lazy saving of FPU
    registers. */
    mov r0, #0
    msr control, r0
    /* Globally enable interrupts. */
    cpsie i
    cpsie f
    dsb
    isb
    /* Call SVC to start the first task. */
    svc 0
    nop
    nop
}
```

## 逐行对应

| 指令 | 功能 |
|---|---|
| `LDR R0,=0xE000ED08` | 取 SCB->VTOR 的寄存器地址 |
| `LDR R0,[R0]` 第一次 | 读当前向量表基地址 |
| `LDR R0,[R0]` 第二次 | 读向量表首字：初始 MSP 值 |
| `MSR MSP,R0` | 重新设置主栈位置，旧的启动调用栈不再需要 |
| `MOV R0,#0 / MSR CONTROL,R0` | 设特权 Thread/MSP 选择，并清理活动 FPU 标记 |
| `CPSIE I` | 清 PRIMASK |
| `CPSIE F` | 清 FAULTMASK；不是打开 FPU 的意思 |
| `DSB / ISB` | 同步控制状态和后续指令执行 |
| `SVC 0` | 请求 SVC 异常，进入第一个任务恢复流程 |
| `NOP / NOP` | 正常进入任务后一般不会返回执行 |

关键提示：原始注释说“NVIC offset register”，但 `0xE000ED08` 是 **SCB->VTOR**。通常 `prvStartFirstTask()` 由 `xPortStartScheduler()` 调用，`SVC` 再进入 `vPortSVCHandler()`，由任务初始栈帧决定第一次恢复的 PC。
