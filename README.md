# 从裸机到 FreeRTOS 内核：Cortex-M4F 上下文切换学习仓库

> 面向 STM32F407 / Cortex-M4F，按「C 代码 → 汇编 → 异常 → 栈帧 → 调度器」逐步学习。  
> 本仓库由一段连续的 FreeRTOS 底层学习问答整理而成，重点是**看懂源码并能自己解释每条汇编为什么存在**。

## 一分钟理解

- **PC（R15）**：控制指令执行位置；函数调用通常由 `BL` 跳转，异常返回可从任务栈恢复 PC。
- **LR（R14）**：普通函数调用时保存返回位置；进入异常后 Handler 的 LR 通常装入特殊 **EXC_RETURN**。
- **PSP / MSP**：任务通常使用 PSP；异常处理使用 MSP。不同任务的 PSP 栈内存相互独立。
- **TCB**：软件任务控制块，首成员 `pxTopOfStack` 保存任务被切出时的栈顶地址。
- **SysTick**：提供节拍、处理延时到期，必要时请求任务调度。
- **PendSV**：保存旧任务的寄存器现场、选择新任务、恢复其现场。
- **SVC**：在这套 Cortex-M4F 移植中用于启动第一个任务。

```text
Task1(Thread / PSP1)
        │  SysTick / taskYIELD / ISR 请求切换
        ▼
硬件异常入栈：R0-R3、R12、任务LR、PC、xPSR → 旧任务 PSP1 栈
        │
        ▼
PendSV(Handler / MSP)：保存 R4-R11、EXC_RETURN、必要的 S16-S31
        │  旧 TCB->pxTopOfStack = 保存后的 R0
        ▼
vTaskSwitchContext()：更新 pxCurrentTCB → Task2_TCB
        │
        ▼
读取新 TCB 栈顶 → 恢复 R4-R11/EXC_RETURN/可选 FPU → 设置 PSP2
        │
        ▼
BX LR (EXC_RETURN) → 硬件恢复 Task2 的 PC 等 → Task2(Thread / PSP2)
```

## 学习路线

| 顺序 | 文档 | 重点 |
|---:|---|---|
| 01 | [CPU 如何执行 C 代码](docs/01-CPU与函数跳转.md) | PC、BL、BX LR、嵌套调用、函数参数 |
| 02 | [寄存器、变量和栈](docs/02-寄存器与局部变量.md) | R0-R15、AAPCS、寄存器分配、栈变量 |
| 03 | [TCB、PSP、MSP](docs/03-TCB_PSP_MSP与任务栈.md) | 一个 PSP 寄存器，多份任务栈、TCB 寻址 |
| 04 | [异常机制](docs/04-SVC_PendSV_SysTick_EXC_RETURN.md) | Thread/Handler、硬件压栈、SVC、PendSV |
| 05 | [PendSV 逐行详解](docs/05-PendSV逐行解析.md) | **用户给出的 ARMCC/RVDS 汇编**，R0-R3 生命周期 |
| 06 | [启动第一个任务](docs/06-首次任务启动与初始栈帧.md) | VTOR → MSP → CONTROL → SVC → PC |
| 07 | [STMDB / LDMIA 地址演算](docs/07-栈地址计算演练.md) | 9 个寄存器 36 字节、低地址到高地址排列 |
| 08 | [亲手做实验](docs/08-实验与调试.md) | 主机 C 仿真、Keil 寄存器观察、常见错误 |
| 09 | [问答速查与自测](docs/09-FAQ与速查.md) | 从「R2 为什么不保存」到自测题 |
| 10 | [自己开发 Mini RTOS 的路线图](docs/10-MiniRTOS开发路线图.md) | 应构建的函数与分阶段验收 |
| 11 | [ARM 汇编语法速查](docs/11-ARM汇编指令速查.md) | `TST/IT/LDR/STMDB/LDMIA/BL/BX` |

原始汇编学习材料：

- [用户提供的 PendSV 代码（逐段附注）](reference/ARMCC5-PendSV原始代码与注释.md)
- [用户提供的首次任务启动代码（逐段附注）](reference/ARMCC5-StartFirstTask原始代码与注释.md)

## 可运行示例（注意区分！）

| 示例 | 运行环境 | 功能 | 是否真正 RTOS 上下文切换 |
|---|---|---|---|
| [pure_c_scheduler](examples/pure_c_scheduler/) | Windows/Linux/macOS 的 C 编译器 | 两个周期性回调任务，纯 C 非抢占调度 | **否** |
| [context_simulation](examples/context_simulation/) | Windows/Linux/macOS 的 C 编译器 | 模拟 R4-R11/LR 压栈与出栈、TCB 栈顶管理 | **否**，仅模拟栈内存 |
| [stm32_register_lab](examples/stm32_register_lab/) | STM32F407 Keil/CubeIDE 的已有工程 | 观察 BL、LR、寄存器和栈访问 | **否**，是反汇编教学代码 |

**为什么没有把 `PendSV_Handler` 当作纯 C 示例直接编译？** 真正切换任务的核心涉及特殊寄存器、异常返回和 ABI 约束，需要专门的 ARM 移植实现。本仓库把原始 ARMCC5 汇编放在 `reference/` 供学习；它依赖完整 FreeRTOS 内核、向量表和编译环境，不能单独复制到普通 C 项目中运行。

### 电脑上快速运行两个教学实验

```bash
# 在仓库根目录
cc -std=c11 -Wall -Wextra -pedantic examples/pure_c_scheduler/main.c -o pure_c_scheduler
./pure_c_scheduler

cc -std=c11 -Wall -Wextra -pedantic examples/context_simulation/main.c -o context_simulation
./context_simulation
```

Windows PowerShell 使用 MinGW GCC 编译后，执行 `./pure_c_scheduler.exe` / `./context_simulation.exe`（也可使用 MSVC）。

## 你最值得记住的五条汇编

```asm
MRS   R0, PSP                  ; 获取旧任务 PSP
STMDB R0!, {R4-R11, R14}       ; 保存软件现场（可选 FPU 另行处理）
STR   R0, [R2]                 ; 更新旧任务 TCB->pxTopOfStack
LDMIA R0!, {R4-R11, R14}       ; 从新任务栈恢复软件现场
MSR   PSP, R0                  ; 切换到新任务 PSP
BX    R14                      ; 根据 EXC_RETURN 异常返回，硬件恢复 PC
```

> `R2` 是异常处理程序的**临时工作寄存器**，指向旧任务的 TCB；任务本来的 R2 已在异常入栈时被硬件保存。调度后读取新任务 TCB 主要通过 `R3→R1→R0`，无需保存 Handler 中旧 R2 的临时值。

## 官方源码入口

- [FreeRTOS-Kernel / portable / RVDS / ARM_CM4F / port.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/portable/RVDS/ARM_CM4F/port.c)（与你提供的 `__asm void` / `PRESERVE8` 风格相符）
- [FreeRTOS-Kernel / portable / GCC / ARM_CM4F / port.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/portable/GCC/ARM_CM4F/port.c)（GCC `__attribute__((naked))` + `__asm volatile`）
- [FreeRTOS-Kernel / tasks.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/tasks.c)（`vTaskSwitchContext()` 在此）
- [ARM Cortex-M4 Devices Generic User Guide](https://developer.arm.com/documentation/dui0553/latest/)（架构寄存器/异常权威参考）

## 如何上传到 GitHub

1. 在 GitHub 新建一个**空仓库**（建议名称 `FreeRTOS-CortexM4-Study`，先不要勾选自动创建 README）。
2. 解压本 ZIP，进入有 `README.md` 的那一层目录，打开 Git Bash 或 PowerShell。
3. 首次上传：

```bash
git init
git add .
git commit -m "docs: add Cortex-M4F FreeRTOS study notes"
git branch -M main
git remote add origin https://github.com/YOUR_USERNAME/FreeRTOS-CortexM4-Study.git
git push -u origin main
```

4. 后面修改笔记并更新：

```bash
git status
git add .
git commit -m "docs: update PendSV notes"
git push
```

如果 `git commit` 提示没有配置身份，按 Git 提示设置 `user.name` / `user.email`；推送需要 GitHub 认证（通常是浏览器登录或凭据管理器）。

## 范围与说明

- 面向 **Cortex-M4F + 常见非 MPU FreeRTOS 移植**。实际源码随 FreeRTOS 版本、编译器、FPU 设置而不同；请以你项目的 `port.c` 为准。
- 文中内存地址、寄存器数值如无明确说明均为教学示例。
- 自编教学代码使用 [MIT License](LICENSE)；引用的原始 FreeRTOS 源码以官方仓库许可为准。`reference/` 内的短代码片段是用户在对话中提供的学习材料。
