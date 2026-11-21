# 11｜看 FreeRTOS PendSV 必备的 ARM/Thumb 汇编语法

> 对照完整寄存器与栈：[02-寄存器](02-寄存器与局部变量.md)、[17-SVC/PendSV 逐条轨迹](17-SVC与PendSV完整汇编寄存器跟踪.md)。

## 1. 寄存器、寻址与伪指令

```asm
MRS R0, PSP              ; 读特殊寄存器 PSP → R0
MSR PSP, R0              ; 写特殊寄存器 R0 → PSP
LDR R3, =pxCurrentTCB    ; 伪指令：取得全局变量地址
LDR R2, [R3]             ; 读取 R3 指向的内存 → R2
STR R0, [R2]             ; 把 R0 写到 R2 指向的内存
MOV R0, #0               ; R0 = 0
```

**`=` 与 `[]` 是不同概念**：`LDR R3,=x` 将 `x` 的地址装进 R3；`LDR R2,[R3]` 取出地址 R3 中的内存值。真实汇编器可能将 `LDR R3,=...` 扩展为字面量池读取等指令。

## 2. 栈操作

```asm
STMDB R0!, {R4-R11,R14}  ; 更新 R0 为更低地址，保存 9 个 32 位寄存器
LDMIA R0!, {R4-R11,R14}  ; 从 R0 位置开始向高地址取 9 个寄存器
PUSH {R4,LR}            ; 使用当前 SP 的栈操作
POP  {R4,PC}            ; 使用当前 SP，读取 PC 可实现普通返回
```

`!` 表示 writeback（基址回写）。FreeRTOS 当前示例 `STMDB R0!` 不直接改 PSP，需后续把新 R0 保存到旧 TCB。

## 3. 条件与 FPU

你最早问过：

```asm
TST R14, #0x10
IT EQ
VSTMDBEQ R0!, {S16-S31}
```

- `TST`：按位与、只更新条件标志，不写回 R14。
- `#0x10`：bit 4 掩码。
- `IT EQ`：让下一条具备 `EQ`（Z=1）条件执行。
- `VSTMDBEQ`：只有 bit4 为 0 时保存 S16-S31。ARMCC 常使用此写法；在新 Thumb 架构/汇编器语法与规则下，IT 限制需要查目标指令集文档。

## 4. 调用、跳转和异常返回

```asm
B   label               ; 分支
BL  vTaskSwitchContext  ; 将返回地址写入 LR，然后调用函数
BX  LR                  ; 在普通函数中根据 LR 返回
BX  R14                 ; 若在 Handler 且 R14=EXC_RETURN，执行异常返回
SVC #0                  ; 主动触发 SVC 异常
```

## 5. 中断屏蔽与屏障

```asm
CPSIE I                 ; 清 PRIMASK，开放可配置优先级异常
CPSIE F                 ; 清 FAULTMASK（不是 enable FPU）
MSR BASEPRI, R0         ; 设置部分异常的优先级屏蔽阈值
DSB                     ; 数据同步屏障
ISB                     ; 指令同步屏障
```

`BASEPRI` 的数值不是简单的“任务优先级”；它是 Cortex-M NVIC 的硬件中断优先级编码。较高的紧急程度通常对应**更小的数字**，具体还受实现位数和分组影响。

## 6. 两个值得避免的误区

1. `R14` 在普通函数中通常保存函数返回地址，但在异常 Handler 中含义可能变为 EXC_RETURN。它保存的是*异常返回方式*，不是某个 Task2 函数入口。
2. `STMDB` 不保证 R4 一定保存变量 `count`，它只保存 R4 的数值；编译器负责变量分配，任务恢复时按原寄存器槽位读取。
