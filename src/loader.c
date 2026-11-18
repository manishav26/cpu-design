
/*
 * loader.c -- Load assembled SIMPLE-16 programs into CPU RAM.
 */

#include <string.h>
#include "loader.h"

int cpu_load_program(CPU *cpu, const Program *prog)
{
    if (cpu == NULL || prog == NULL)
        return -1;

    /* Reset registers, flags, PC, SP, and RAM. */
    cpu_reset(cpu);

    
    memcpy(cpu->ram, prog->mem, sizeof(cpu->ram));

    
    cpu->pc = TEXT_START;

    return 0;
}

