# STM32F407 寄存器观察代码（需要导入已有工程）

`task_register_lab.c` 只提供 3 个教学函数，不是完整 STM32 工程，也不依赖 HAL。将它加入正常可编译的 STM32F407 工程，在 `main()` 初始化完成后调用 `RegisterLab_Run()`。

在 Keil 的反汇编窗口观察 `RegisterLab_Add` 和 `RegisterLab_Nested`：

- 函数参数通常通过 R0/R1 传递。
- 常见整数返回值在 R0。
- `BL` 会更新 LR，嵌套调用时需遵守 ABI 保存返回地址。
- 局部变量可能分配到寄存器或函数栈。

建议使用低优化等级防止函数内联，但不同编译器生成的机器指令并不完全相同。
