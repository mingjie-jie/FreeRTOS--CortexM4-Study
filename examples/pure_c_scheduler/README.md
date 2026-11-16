# 纯 C 双任务周期调度器（可运行）

使用 C11，运行与芯片无关，不需要 FreeRTOS：

```bash
cc -std=c11 -Wall -Wextra -pedantic main.c -o scheduler_demo
./scheduler_demo
```

任务均为可返回的小函数，调度器按到期时间和优先级执行。Task1 周期 500ms，Task2 周期 1000ms；周期由模拟时间提供，非真实墙上时间。

**它不是抢占式内核！** 没有任务独立栈、PSP、PC 保存、PendSV。目的只是观察 `task_func()` 与 FreeRTOS 上下文恢复之间的区别。
