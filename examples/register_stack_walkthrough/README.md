# 纯 C 寄存器栈帧演算

本例不接触 ARM CPU 真正的 PSP/R0/PC，而是把模拟 RAM 写入一个 C 数组，展示：

1. 硬件自动保存的基本异常栈帧（8 个字，R2 位于 `+2 words`，PC 位于 `+6 words`）；
2. PendSV 软件保存的 R4～R11、EXC_RETURN（9 个字）；
3. Handler 临时修改 R2 **不会覆盖栈中保存的任务原 R2**；
4. 从不同栈区按位置恢复寄存器，而不是根据 C 变量名匹配。

编译：

```bash
cc -std=c11 -Wall -Wextra -Werror -pedantic main.c -o regframe
./regframe
```

对应图文说明：[docs/13-寄存器现场与异常栈帧详图.md](../../docs/13-寄存器现场与异常栈帧详图.md)。
