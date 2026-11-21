# v2.0 示例测试记录

测试环境：Linux 主机标准 C 编译器；CMake/CTest 编译运行。

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

| 测试 | 验证内容 | 结果 |
|---|---|---|
| `pure_c_scheduler_test` | 两个任务的纯 C 周期性回调调度 | PASS |
| `context_simulation_test` | 保存/恢复每任务的寄存器镜像 | PASS |
| `register_stack_walkthrough_test` | 硬件基本帧+软件帧布局，原 R2/PC 恢复 | PASS |
| `delay_state_machine_test` | READY/BLOCKED、到期唤醒和 Idle 执行 | PASS |

**结果：4/4 tests passed。**

这些仅是电脑上运行的 C 语言教学测试；并没有在本次测试中对 STM32F407 的 SVC/PendSV/FPU 进行硬件在板验证，也不能替代真实移植层中断优先级、异常栈对齐和 FPU lazy stacking 的调试。
