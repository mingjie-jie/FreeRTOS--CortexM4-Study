# 从裸机到 FreeRTOS 内核：Cortex-M4F 底层学习工程

**版本：v2.0（2026-11-21）｜适配 STM32F407 / Cortex-M4F / Keil ARM Compiler 5（也列出 GCC 参考路径）**

这套仓库整理了一整段从“CPU 的 PC 为什么能顺序执行”到“如何自己实现 Mini RTOS 的双任务切换和 OS_Delay”的学习过程。以**寄存器、栈、TCB、异常硬件、汇编逐条追踪**为主线，既适合第一次理解底层机制，也适合反复查阅。所有说明和注释都是中文。

> **阅读重点**：从寄存器入手，建议先读 [02](docs/02-寄存器与局部变量.md) → [13](docs/13-寄存器现场与异常栈帧详图.md) → [17](docs/17-SVC与PendSV完整汇编寄存器跟踪.md)，然后再读异常和调度。旧版本过于简略的问题在 v2.0 中已重点加强。

## 一、快速导航：你想查什么？

| 你的疑问 | 直达笔记 |
|---|---|
| R0～R15 每个寄存器分别干什么？ | **[02-寄存器与局部变量](docs/02-寄存器与局部变量.md)** |
| R2 为什么不保存、R3 为什么一直保留？ | [02 第 6 节](docs/02-寄存器与局部变量.md) · **[17-汇编寄存器跟踪](docs/17-SVC与PendSV完整汇编寄存器跟踪.md)** |
| STM32 的变量到底在哪个寄存器？ | **[12-Keil 寄存器分配实验](docs/12-寄存器分配与反汇编实验.md)** |
| 任务 R2/PC 与 Handler 临时 R2 的区别？ | **[13-完整栈帧+地址](docs/13-寄存器现场与异常栈帧详图.md)** |
| MSP/PSP/CONTROL/xPSR/BASEPRI 是什么？ | [03-TCB/PSP/MSP](docs/03-TCB_PSP_MSP与任务栈.md) · **[14-特殊寄存器](docs/14-CortexM特殊寄存器与中断屏蔽.md)** |
| `STMDB`/`LDMIA` 从哪里开始存、怎么取？ | [07-栈地址计算](docs/07-栈地址计算演练.md) · [13-栈帧图](docs/13-寄存器现场与异常栈帧详图.md) |
| PC 是怎么被 SVC 恢复成任务入口地址的？ | [06-首次任务启动](docs/06-首次任务启动与初始栈帧.md) · [17-汇编跟踪](docs/17-SVC与PendSV完整汇编寄存器跟踪.md) |
| `osDelay()` 到底怎样让出 CPU？ | **[15-OS_Delay 阻塞/唤醒](docs/15-osDelay阻塞唤醒与系统Tick.md)** |
| 想自己写两个任务的 Mini RTOS？ | [10-开发路线](docs/10-MiniRTOS开发路线图.md) · [16-端到端链路](docs/16-从创建任务到延时唤醒完整链路.md) |
| 看不懂 Thumb 汇编？ | [11-汇编速查](docs/11-ARM汇编指令速查.md) · [18-练习与答案](docs/18-寄存器与栈计算练习题.md) |

## 二、完整章节目录

### A. 基础与寄存器

1. [01｜CPU 与 C 语言函数跳转](docs/01-CPU与函数跳转.md)：PC、BL/BX、参数、返回和裸机运行原理。
2. **[02｜寄存器深度解析（v2 重写）](docs/02-寄存器与局部变量.md)**：R0～R15 逐一介绍；AAPCS；寄存器溢出；栈局部变量；FPU；R2/R3 解疑。
3. [03｜TCB、PSP、MSP 与任务栈](docs/03-TCB_PSP_MSP与任务栈.md)：一套 PSP、多个任务栈；TCB 指针多次解引用。
4. [04｜SVC、PendSV、SysTick、EXC_RETURN](docs/04-SVC_PendSV_SysTick_EXC_RETURN.md)：异常入口和异常返回。

### B. FreeRTOS 汇编源码

5. [05｜PendSV 核心汇编逐行解析](docs/05-PendSV逐行解析.md)：R2/旧 TCB、R3/全局当前指针和调度保护。
6. [06｜首次启动 SVC 与人工任务栈](docs/06-首次任务启动与初始栈帧.md)：PC 如何在第一次运行前被预填入栈帧。
7. [07｜STMDB/LDMIA 地址演算](docs/07-栈地址计算演练.md)：九个寄存器保存 36 字节。
8. [08｜调试实验](docs/08-实验与调试.md)：主机模拟、Keil/CubeIDE 实测。
9. [09｜高频问答](docs/09-FAQ与速查.md)：对话中的核心疑问归档。
10. [10｜Mini RTOS 开发路线图](docs/10-MiniRTOS开发路线图.md)：从手动 yield 到 Tick、Idle 和抢占。
11. [11｜ARM 汇编指令速查](docs/11-ARM汇编指令速查.md)：`MRS/MSR/LDR/STR/TST/IT/STMDB/LDMIA/BL/BX`。

### C. v2.0 新增深度讲解

12. **[12｜寄存器分配与反汇编实验](docs/12-寄存器分配与反汇编实验.md)**：6 组 Keil 观察实验。
13. **[13｜完整寄存器现场与栈帧图](docs/13-寄存器现场与异常栈帧详图.md)**：硬件 32B + 软件 36B 的逐地址表；Task1 原 R2 和 Handler 临时 R2。
14. **[14｜Cortex-M 特殊寄存器](docs/14-CortexM特殊寄存器与中断屏蔽.md)**：CONTROL、xPSR、BASEPRI、PRIMASK、FAULTMASK、VTOR、ICSR、FPU。
15. **[15｜osDelay 与阻塞唤醒](docs/15-osDelay阻塞唤醒与系统Tick.md)**：Tick、延时列表、Idle、临界区和回绕。
16. **[16｜创建任务到延时唤醒全过程](docs/16-从创建任务到延时唤醒完整链路.md)**：xTaskCreate→SVC→PendSV→osDelay。
17. **[17｜SVC/PendSV 汇编寄存器时间轴](docs/17-SVC与PendSV完整汇编寄存器跟踪.md)**：逐指令追踪 R0、R1、R2、R3 与 RAM 地址变化。
18. [18｜15 道寄存器与栈计算题](docs/18-寄存器与栈计算练习题.md)：答案折叠，复习时可自测。

## 三、源码原文及参考

- [ARMCC5 PendSV 汇编与注释](reference/ARMCC5-PendSV原始代码与注释.md)
- [ARMCC5 prvStartFirstTask 汇编与注释](reference/ARMCC5-StartFirstTask原始代码与注释.md)
- [ARMCC5 SVC Handler 深度跟踪](docs/17-SVC与PendSV完整汇编寄存器跟踪.md)
- [官方 RVDS/ARM_CM4F/port.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/portable/RVDS/ARM_CM4F/port.c)
- [官方 GCC/ARM_CM4F/port.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/portable/GCC/ARM_CM4F/port.c)
- [官方 tasks.c](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/tasks.c)
- [ARM Cortex-M4 Generic User Guide](https://documentation-service.arm.com/static/5f2ac4ab60a93e65927bbdbf)
- [ARM AN298：FPU lazy stacking / context switching](https://documentation-service.arm.com/static/5f0352c0cafe527e86f5bc66)

**版本注意**：`__asm void` + `PRESERVE8` 是 ARMCC5 风格；GCC 通常在 `port.c` 内使用 `__asm volatile` / `naked`。同一套 CPU 机制，不能把两种编译器语法直接混用。实际 `port.c` 随 FreeRTOS 版本/配置可能有所不同。

## 四、可编译、已测试的主机 C 示例

| 工程 | 作用 | 真正的 Cortex-M 上下文切换？ |
|---|---|---|
| [pure_c_scheduler](examples/pure_c_scheduler/) | 两个周期性回调的非抢占式调度 | 否 |
| [context_simulation](examples/context_simulation/) | 模拟独立任务寄存器镜像和 TCB | 否 |
| **[register_stack_walkthrough](examples/register_stack_walkthrough/)** | 模拟基本异常帧 + 软件现场；验证 R2/PC 的槽位和恢复 | 否，v2 新增 |
| **[delay_state_machine](examples/delay_state_machine/)** | 模拟阻塞、Tick 到期、Idle | 否，v2 新增 |
| [stm32_register_lab](examples/stm32_register_lab/) | 可导入已有 Keil/CubeIDE STM32 工程观察反汇编 | 非 RTOS 实验，不能独立烧写 |

命令：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

v2.0 当前四个主机 C 示例都已通过构建与测试；详见 [TEST_RESULTS.md](TEST_RESULTS.md)。**这不代表已经做过 STM32 硬件的真实 PendSV / FPU 测试。**

## 五、如何使用、上传与更新 GitHub

1. 解压仓库 ZIP，进入含 `README.md` 的目录。
2. 在 GitHub 新建一个空仓库，例如 `FreeRTOS-CortexM4-Study`。
3. 第一次上传：

```bash
git init
git add .
git commit -m "docs: add FreeRTOS Cortex-M4F study v2"
git branch -M main
git remote add origin https://github.com/YOUR_USERNAME/FreeRTOS-CortexM4-Study.git
git push -u origin main
```

**若你已经在 GitHub 上传过 v1，不要重新初始化远程仓库，也不用删除原有仓库**。在你本机已经 `git clone` 的仓库目录中，用 v2 解压得到的文件覆盖**对应工作区文件**（保留 `.git/`），然后：

```bash
git status
git add .
git commit -m "docs: expand Cortex-M4 registers, exception stacks and osDelay"
git push
```

> 注意：本 ZIP **没有 `.git/` 历史**，所以直接解压到新目录并不能自动继承你已有仓库的提交历史。最佳做法是在已有仓库的工作目录中更新文件，再提交。

## 六、复习建议

- **第一轮**：02 → 03 → 13（只围绕寄存器和栈）
- **第二轮**：04 → 06 → 17 → 05（从 SVC 首次启动到 PendSV 切换）
- **第三轮**：07 → 12 → 18（亲手做地址演算与反汇编）
- **第四轮**：10 → 15 → 16（扩展 Mini RTOS 和阻塞延时）

本仓库保留原始教学代码，所有主机模拟均明确标记“非真实抢占式 RTOS”。用于教学、面试准备、GitHub 学习记录；实际项目上的 CPU 低层异常代码请以**目标芯片、所用编译器、FreeRTOS 版本与 FPU/MPU 配置**为准。

参见 [CHANGELOG.md](CHANGELOG.md) 查看版本差异。自编代码采用 [MIT License](LICENSE)；引用的官方 FreeRTOS 源码遵守原项目许可。
