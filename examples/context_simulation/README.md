# 纯 C 上下文栈模拟器（可运行）

```bash
cc -std=c11 -Wall -Wextra -pedantic main.c -o context_demo
./context_demo
```

把 R4～R11 和 Handler LR (EXC_RETURN) 抽象为 9 个 `uint32_t`，用数组模拟 `STMDB` 先减小指针、`LDMIA` 向高地址读取，模拟在两个 TCB 之间切换寄存器镜像。

**它不做真正的 ARM 异常返回**，不会访问 PSP、MSP、PC，且没有覆盖硬件异常帧、FPU 扩展帧等真实细节。可以用于理解“寄存器值放在哪、下次按什么顺序拿回来”。
