# OS_Delay 任务状态模拟

通过纯 C 在电脑上模拟 Tick、BLOCKED/READY、定时唤醒和 Idle 兜底行为。**这不是一个真正的多栈抢占式 RTOS**：回调结束后返回调度循环，不支持从任意机器指令处恢复 PC。

编译：

```bash
cc -std=c11 -Wall -Wextra -Werror -pedantic main.c -o delay_demo
./delay_demo
```

对应：[docs/15-osDelay阻塞唤醒与系统Tick.md](../../docs/15-osDelay阻塞唤醒与系统Tick.md)。
