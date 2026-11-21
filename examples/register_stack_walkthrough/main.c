/*
 * CPU register frame layout walkthrough -- standard C model, not real ARM registers.
 * Models the no-FPU case: simulated hardware exception stacking +
 * STMDB R0!,{R4-R11,LR} + LDMIA + exception return.
 */
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

/* 32-bit words representing SRAM. SP is an index into this array. */
enum { WORDS = 80, CORE_WORDS = 9, BASIC_WORDS = 8 };

typedef struct {
    uint32_t ram[WORDS];
    size_t sp;
} Stack;

typedef struct {
    uint32_t r[16];
    uint32_t xpsr;
} CPU;

static void stack_init(Stack *s) { s->sp = WORDS; }

/* Hardware basic frame R0,R1,R2,R3,R12,LR,PC,xPSR, low->high address. */
static void hw_stack(Stack *s, const CPU *c)
{
    assert(s->sp >= BASIC_WORDS);
    s->sp -= BASIC_WORDS;
    for (size_t i = 0; i < 4; ++i) s->ram[s->sp + i] = c->r[i];
    s->ram[s->sp + 4] = c->r[12];
    s->ram[s->sp + 5] = c->r[14];
    s->ram[s->sp + 6] = c->r[15];
    s->ram[s->sp + 7] = c->xpsr;
}

/* Software STMDB R0!,{R4-R11,R14}; R14 is Handler's EXC_RETURN. */
static void sw_stack(Stack *s, const CPU *c, uint32_t exc_return)
{
    assert(s->sp >= CORE_WORDS);
    s->sp -= CORE_WORDS;
    for (size_t i = 0; i < 8; ++i) s->ram[s->sp + i] = c->r[i + 4];
    s->ram[s->sp + 8] = exc_return;
}

static void sw_unstack(Stack *s, CPU *c, uint32_t *exc_return)
{
    for (size_t i = 0; i < 8; ++i) c->r[i + 4] = s->ram[s->sp + i];
    *exc_return = s->ram[s->sp + 8];
    s->sp += CORE_WORDS;
}

static void hw_unstack(Stack *s, CPU *c)
{
    for (size_t i = 0; i < 4; ++i) c->r[i] = s->ram[s->sp + i];
    c->r[12] = s->ram[s->sp + 4];
    c->r[14] = s->ram[s->sp + 5];
    c->r[15] = s->ram[s->sp + 6];
    c->xpsr = s->ram[s->sp + 7];
    s->sp += BASIC_WORDS;
}

int main(void)
{
    Stack s;
    CPU original = {0}, restored = {0};
    uint32_t exc_return = 0;
    stack_init(&s);
    original.r[2] = UINT32_C(0x12345678); /* Task1 original R2 */
    original.r[4] = 11;
    original.r[5] = 31;
    original.r[14] = UINT32_C(0x080000F1); /* Task1 original LR */
    original.r[15] = UINT32_C(0x08003456); /* Task1 resumed PC */
    original.xpsr = UINT32_C(0x01000000);

    hw_stack(&s, &original);
    size_t basic_start = s.sp;
    assert(s.ram[basic_start + 2] == UINT32_C(0x12345678));
    assert(s.ram[basic_start + 6] == UINT32_C(0x08003456));
    printf("Basic frame: index=%zu, R2 slot=%zu, PC slot=%zu\n",
           basic_start, basic_start + 2, basic_start + 6);

    sw_stack(&s, &original, UINT32_C(0xFFFFFFFD));
    size_t tcb_saved_sp = s.sp;
    printf("TCB saved SP index=%zu (9 software words below basic frame)\n", tcb_saved_sp);
    assert(tcb_saved_sp + CORE_WORDS == basic_start);
    assert(s.ram[tcb_saved_sp + 0] == 11); /* R4 */
    assert(s.ram[tcb_saved_sp + 1] == 31); /* R5 */
    assert(s.ram[tcb_saved_sp + 8] == UINT32_C(0xFFFFFFFD));

    /* In a real PendSV Handler, physical R2 can be repurposed as TCB address. */
    uint32_t handler_r2 = UINT32_C(0x20000100);
    assert(handler_r2 != original.r[2]);
    assert(s.ram[basic_start + 2] == original.r[2]);

    sw_unstack(&s, &restored, &exc_return);
    assert(exc_return == UINT32_C(0xFFFFFFFD));
    hw_unstack(&s, &restored);
    assert(s.sp == WORDS);
    assert(restored.r[2] == original.r[2]);
    assert(restored.r[4] == original.r[4]);
    assert(restored.r[5] == original.r[5]);
    assert(restored.r[14] == original.r[14]);
    assert(restored.r[15] == original.r[15]);
    puts("PASS: R2, R4, R5, task LR and task PC restored correctly.");
    return 0;
}
