/*
 * 教学实验 C：将本文件加入已有的 STM32F407 Keil/CubeIDE 工程。
 * 它不是独立的板级工程，也不实现 RTOS 上下文切换。
 * 在 main() 完成常规初始化后调用 RegisterLab_Run()，查看反汇编。
 * 优先使用 -O0 并确保编译器未内联教学函数。
 */
#include <stdint.h>

#if defined(__GNUC__) || defined(__CC_ARM) || defined(__ARMCC_VERSION)
# define LAB_NOINLINE __attribute__((noinline))
#else
# define LAB_NOINLINE
#endif

static volatile uint32_t g_lab_result;

LAB_NOINLINE uint32_t RegisterLab_Add(uint32_t a, uint32_t b)
{
    uint32_t result = a + b;
    g_lab_result = result;        /* 让计算结果具有可观察副作用 */
    return result;
}

LAB_NOINLINE uint32_t RegisterLab_Nested(uint32_t a)
{
    uint32_t temporary = RegisterLab_Add(a, 20u);
    return temporary * 2u;
}

void RegisterLab_Run(void)
{
    volatile uint32_t a = 10u;
    volatile uint32_t b = 20u;
    uint32_t sum = RegisterLab_Add(a, b);
    uint32_t nested = RegisterLab_Nested(sum);
    g_lab_result = nested;
    /* 设断点观察 R0、R1、LR、PC、SP 与 g_lab_result */
}
