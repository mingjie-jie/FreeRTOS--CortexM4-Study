/*
 * 教学实验 B：纯 C 模拟 STMFD/STMDB 与 LDMIA 的内存效果。
 * 这个程序不访问 CPU 的真实 R4-R11/PSP，也不是实际任务切换。
 */
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

enum { STACK_WORDS = 64, SAVED_CORE_REGS = 9 };

/* 按 R4,R5,...,R11,LR 顺序存放，每项为 32 位。 */
typedef struct {
    uint32_t value[SAVED_CORE_REGS];
} RegisterImage;

typedef struct {
    const char *name;
    uint32_t stack[STACK_WORDS];
    size_t sp_index;   /* 模拟 TCB->pxTopOfStack 所表示的位置 */
} SimTCB;

static void init_tcb(SimTCB *task, const char *name)
{
    task->name = name;
    task->sp_index = STACK_WORDS;
    for (size_t i = 0; i < STACK_WORDS; ++i) {
        task->stack[i] = 0u;
    }
}

/* STMDB R0!,{R4-R11,LR} 的简化内存模型：SP 减 9 个字，再按编号递增放入。 */
static void save_core(SimTCB *task, const RegisterImage *cpu)
{
    assert(task->sp_index >= SAVED_CORE_REGS);
    task->sp_index -= SAVED_CORE_REGS;
    for (size_t i = 0; i < SAVED_CORE_REGS; ++i) {
        task->stack[task->sp_index + i] = cpu->value[i];
    }
}

/* LDMIA R0!,{R4-R11,LR} 的简化内存模型。 */
static void restore_core(SimTCB *task, RegisterImage *cpu)
{
    assert(task->sp_index + SAVED_CORE_REGS <= STACK_WORDS);
    for (size_t i = 0; i < SAVED_CORE_REGS; ++i) {
        cpu->value[i] = task->stack[task->sp_index + i];
    }
    task->sp_index += SAVED_CORE_REGS;
}

static void show(const char *stage, const SimTCB *task, const RegisterImage *cpu)
{
    printf("%-12s | %-5s | R4=%" PRIu32 " R5=%" PRIu32 " | stack index=%zu\n",
           stage, task->name, cpu->value[0], cpu->value[1], task->sp_index);
}

int main(void)
{
    SimTCB task1, task2;
    RegisterImage cpu = {{0}};       /* CPU 物理寄存器镜像只有一套 */
    RegisterImage image1 = {{11u, 31u, 0u, 0u, 0u, 0u, 0u, 0u, 0xFFFFFFFDu}};
    RegisterImage image2 = {{100u, 200u, 0u, 0u, 0u, 0u, 0u, 0u, 0xFFFFFFFDu}};

    init_tcb(&task1, "Task1");
    init_tcb(&task2, "Task2");
    save_core(&task1, &image1);
    save_core(&task2, &image2);
    puts("=== Simulated stack save/restore (NOT Cortex-M exception) ===");
    puts("Low index holds R4, then R5 ... R11, finally EXC_RETURN.");

    restore_core(&task1, &cpu);
    show("run", &task1, &cpu);
    cpu.value[0] += 1u;      /* Task1 模拟执行 count++ */
    cpu.value[1] += cpu.value[0];
    save_core(&task1, &cpu); /* Task1 暂停 */
    show("save", &task1, &cpu);

    restore_core(&task2, &cpu);
    show("run", &task2, &cpu);
    cpu.value[0] += 10u;
    cpu.value[1] += cpu.value[0];
    save_core(&task2, &cpu); /* Task2 暂停 */
    show("save", &task2, &cpu);

    restore_core(&task1, &cpu);
    show("resume", &task1, &cpu);
    assert(cpu.value[0] == 12u && cpu.value[1] == 43u);
    assert(cpu.value[8] == 0xFFFFFFFDu);
    puts("PASS: Task1 R4/R5 were correctly restored after Task2 ran.");
    return 0;
}
