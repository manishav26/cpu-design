
#ifndef LOADER_H
#define LOADER_H

#include "cpu.h"
#include "assembler.h"

/*
 * Load an assembled program into CPU memory.
 *
 * Returns:
 *   0  = success
 *  -1  = invalid arguments
 */
int cpu_load_program(CPU *cpu, const Program *prog);

#endif
