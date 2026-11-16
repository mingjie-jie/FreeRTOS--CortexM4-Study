# 示例测试记录

打包前在 Linux 环境中执行：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

结果：`2/2 tests passed`。

- `pure_c_scheduler`：输出 Task1 在 500/1000/1500/2000/2500 ms 执行（5 次）；Task2 在 1000/2000 ms 执行（2 次）。相同时间点优先级更高的 Task2 先运行。
- `context_simulation`：模拟 Task1 R4=11、R5=31 → 运行后 R4=12、R5=43；切走并运行 Task2 后再恢复 Task1，断言 Task1 原数值保持正确。

以上只是主机 C 教学实验的测试结果，**并非 STM32F407 硬件测试**，也不验证真实 PendSV/SVC/FPU 异常切换。
