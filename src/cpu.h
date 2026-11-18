
#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>
#include "isa.h"

/*
 * Simple16 CPU state
 * Shared with the execution engine and trace module.
 */
typedef struct {
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

    /* Status flags */
    uint8_t z;  /* Zero */
    uint8_t n;  /* Negative */

    /* Execution status */
    bool halted;
    bool faulted;

    /* Total executed instructions */
    unsigned long steps;

} CPU;

/* Reset CPU and clear memory */
void cpu_reset(CPU *cpu);

/* Execute one instruction */
int cpu_step(CPU *cpu);

/* Execute until HALT, error, or step limit */
int cpu_run(CPU *cpu, unsigned long max_steps);

#endif
