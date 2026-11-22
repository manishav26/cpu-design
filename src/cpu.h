/*
 * cpu.h -- SIMPLE-16 CPU state: registers, ALU, bus, RAM and clock.
 *
 *   +-------------------------- CPU --------------------------+
 *   |  GPR: R0 R1 R2 R3     SPR: PC IR SP MAR MDR FLAGS(Z,N)  |
 *   |  ALU: a (op) b -> out           clock: FETCH/DECODE/EXEC |
 *   +---------------------------+------------------------------+
 *                               |  Bus: address / data / control (R or W)
 *                       +-------+-------+
 *                       |  RAM 256 x 16 |
 *                       +---------------+
 */
#ifndef CPU_H
#define CPU_H

#include <stdbool.h>
#include <stdint.h>
#include "isa.h"

/* The bus between the CPU and RAM.
 * address = driven from MAR, data = carried to/from MDR,
 * control = 'R' (read), 'W' (write) or '-' (idle in this clock phase). */
typedef struct {
    uint16_t address;
    uint16_t data;
    char     control;
} Bus;

/* The ALU latch: last operation the ALU performed in this instruction. */
typedef struct {
    bool     used;      /* false if this instruction did not use the ALU */
    char     op;        /* '+', '-' or '*' */
    uint16_t a, b;      /* inputs */
    uint16_t out;       /* 16-bit result (also sets Z and N) */
} Alu;

/* One clock tick = one phase of the instruction cycle. */
typedef enum { PHASE_FETCH, PHASE_DECODE, PHASE_EXECUTE } Phase;

struct CPU;
/* Optional callback run after every clock phase (used by the tracer). */
typedef void (*TickHook)(const struct CPU *cpu, Phase phase, void *ctx);

typedef struct CPU {
    /* General-purpose registers */
    uint16_t regs[NUM_REGS];

    /* Memory: 256 words, each 16 bits */
    uint16_t ram[MEM_SIZE];

    /* Special-purpose registers */
    uint16_t pc;
    uint16_t ir;
    uint16_t mar;
    uint16_t mdr;
    uint16_t sp;

    /* Status flags (set by the ALU) */
    uint8_t z;  /* Zero */
    uint8_t n;  /* Negative */

    /* Datapath parts shown by the tracer */
    Bus    bus;     /* last bus transfer in the current phase */
    Alu    alu;     /* last ALU operation */
    Fields dec;     /* fields decoded from IR in the DECODE phase */

    /* Execution status */
    bool halted;
    bool faulted;

    /* Counters */
    unsigned long steps;    /* instructions completed */
    unsigned long clock;    /* clock ticks: 3 per instruction */

    /* Called after each phase; NULL = no tracing */
    TickHook on_tick;
    void    *tick_ctx;
} CPU;

/* Reset CPU and clear memory */
void cpu_reset(CPU *cpu);

/* Execute one instruction: FETCH, DECODE, EXECUTE (3 clock ticks) */
int cpu_step(CPU *cpu);

/* Execute until HALT, error, or step limit */
int cpu_run(CPU *cpu, unsigned long max_steps);

#endif