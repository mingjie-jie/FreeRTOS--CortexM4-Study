# 03｜TCB、PSP、MSP、pxCurrentTCB：谁保存什么？

> **新增扩展阅读**：[13-逐字节栈帧图](13-寄存器现场与异常栈帧详图.md)、[17-SVC/PendSV 的寄存器时间轴](17-SVC与PendSV完整汇编寄存器跟踪.md)。

## 一句话区分

- **TCB（Task Control Block）**：任务的管理结构体，属于内存中的软件数据。
- **PSP（Process Stack Pointer）**：CPU 中的特殊寄存器，指向 Thread Mode 当前使用的任务栈（如果选用 PSP）。
- **MSP（Main Stack Pointer）**：CPU 中的特殊寄存器，Handler Mode 始终使用 MSP；系统复位时也通常使用 MSP。
- **`pxCurrentTCB`**：全局指针变量，其**值**是当前任务 TCB 的地址。
- **`pxTopOfStack`**：TCB 中的成员，保存任务切走后对应的**保存现场栈顶**（不是函数名称或 PC 本身）。
- **PC**：CPU 代码执行位置；在异常基本栈帧中自动保存，可以随任务栈恢复。

## 1. 示例结构体（并非 FreeRTOS 源码完整定义）

```c
typedef struct TCB {
    uint32_t *pxTopOfStack;  // 典型移植要求它是第一个成员（偏移 0）
    uint32_t priority;
    uint32_t *stack_base;
} TCB_t;

TCB_t *pxCurrentTCB;
```

实际 FreeRTOS 的 TCB 在 `tasks.c`，含链表项、任务名称、优先级等很多字段，定义依赖配置。`pxTopOfStack` 位于首成员，才能让汇编直接 `STR R0, [R2]`。

## 2. 内存布局示意

```text
全局 RAM
  0x20000020 : pxCurrentTCB = 0x20000100
                                      │
                                      ▼
  0x20000100 : Task1_TCB.pxTopOfStack = 0x20000FDC
                                      │
                                      ▼
  0x20000FDC : Task1 软件保存的 R4 槽
      ...       R5 ... R11 / EXC_RETURN / 硬件异常栈帧

另一个 TCB 和 Stack2 一直保存在另一块 RAM 上。
```

**一个 CPU 只有一套 PSP 寄存器**；并非“每个任务各有一个 PSP 硬件寄存器”。各任务的 TCB 保存各自离线状态的 `pxTopOfStack`，真正运行时将 PSP 改为选中任务的栈。

## 3. 汇编里的三级取值

```asm
LDR R3, =pxCurrentTCB   ; R3 = &pxCurrentTCB（变量本身的地址）
LDR R2, [R3]            ; R2 = pxCurrentTCB（旧任务 TCB 地址）
STR R0, [R2]            ; oldTCB->pxTopOfStack = R0
...
BL vTaskSwitchContext   ; 可能改变 pxCurrentTCB 的值
...
LDR R1, [R3]            ; R1 = 新的 pxCurrentTCB（新任务 TCB 地址）
LDR R0, [R1]            ; R0 = newTCB->pxTopOfStack
```

### 为什么 R2 后来不保存？

R2 只用来 `STR R0, [R2]`，任务旧栈顶已经写入 TCB 的 RAM。调用调度器后，需要重新读取更新后的 `pxCurrentTCB` 指向谁；保留的是 `R3`（全局变量位置），而非旧 R2（旧 TCB）。任务自身的 R2 原值已经在异常基本栈帧中保存，二者不是一个概念。

### 为什么 `STMDB SP!, {R0,R3}` 在 Handler 里使用 MSP？

Cortex-M4 Handler Mode 使用 MSP；这对指令仅为调用 C 调度函数临时保留 Handler 自己的寄存器值，并维护 8 字节栈对齐。它不是在复制任务的全部局部变量，也不是在保存 Task1 的 R0/R3 原值（这些原值已由硬件自动压在 PSP 栈上）。

## 4. 切换总结

```text
当前 Task1：硬件压栈 → 软件压栈 → TCB1.pxTopOfStack = 新栈顶
选择 Task2：pxCurrentTCB = &TCB2
恢复 Task2：R0 = TCB2.pxTopOfStack → 软件出栈 → PSP = R0
异常返回：CPU 从 PSP 恢复 PC/R0-R3/... → 继续 Task2
```
