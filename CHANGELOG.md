# 更新记录

## v2.0 — 2026-11-21

**保留 v1 原有 25 个文件的核心目录结构并重点扩充：**

- 重写 `docs/02-寄存器与局部变量.md`：详细列出 R0～R15、AAPCS、寄存器溢出、SP/PSP/MSP、R14 的多重含义、变量地址和恢复方式、FPU。
- 新增 `docs/12`：Keil 反汇编、调用 ABI、寄存器与栈槽实验。
- 新增 `docs/13`：不含 FPU 的硬件栈帧 32 字节 + 软件栈帧 36 字节，用具体 RAM 地址完整计算。
- 新增 `docs/14`：CONTROL、xPSR、PRIMASK、BASEPRI、FAULTMASK、VTOR、ICSR 与浮点状态。
- 新增 `docs/15`：osDelay → vTaskDelay → 阻塞 → Tick 到期 → 恢复 PC 的流程。
- 新增 `docs/16`：从任务创建到启动、任务切换、延时唤醒的端到端关系。
- 新增 `docs/17`：SVC/PendSV 逐条汇编寄存器变化时间轴。
- 新增 `docs/18`：15 道带答案的练习题。
- 新增 `examples/register_stack_walkthrough`：纯 C 栈帧布局模拟与原 R2 恢复断言。
- 新增 `examples/delay_state_machine`：纯 C 就绪/阻塞/唤醒/Idle 模拟。
- 更新根 `README.md` 导航、使用说明、已有 GitHub 仓库更新方法。
- 更新 CMake：四组主机 C 示例均带 CI/CTest 检查。

> 注意：新增的主机模拟**不是可直接烧写的 Cortex-M4F 抢占式内核**。原始 ARMCC5 片段仍为参考材料。

## v1.0 — 2026-11-16

初版：11 章学习笔记、两个主机 C 示例、STM32 寄存器实验代码、ARMCC5 参考片段与 GitHub 上传指南。
