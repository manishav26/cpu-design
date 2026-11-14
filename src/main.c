
/*
 * main.c -- SIMPLE-16 CPU simulator entry point.
 *
 * Usage:
 *   ./simple16 program.asm
 */

#include <stdio.h>
#include <stdlib.h>

#include "assembler.h"
#include "cpu.h"
#include "loader.h"

#define MAX_STEPS 10000UL

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s program.asm\n", argv[0]);
        return 1;
    }

    /* Program contains 256 memory words and a listing. */
    static Program program;
    static CPU cpu;

    char error[256];

    /* Assemble the source file. */
    if (assemble_file(argv[1], &program,
                      error, sizeof(error)) != 0) {
        fprintf(stderr, "Assembler error: %s\n", error);
        return 1;
    }

    /* Load assembled machine code into CPU RAM. */
    if (cpu_load_program(&cpu, &program) != 0) {
        fprintf(stderr, "Failed to load program.\n");
        return 1;
    }

    printf("SIMPLE-16 CPU Simulator\n");
    printf("-----------------------\n");


    if (cpu_run(&cpu, MAX_STEPS) != 0) {
        fprintf(stderr, "CPU execution failed.\n");
        return 1;
    }

    printf("Execution completed successfully.\n");
    printf("Instructions executed: %lu\n", cpu.steps);

    printf("\nRegisters:\n");
    for (int i = 0; i < NUM_REGS; i++)
        printf("R%d = %u (0x%04X)\n",
               i, cpu.regs[i], cpu.regs[i]);

    printf("\nRAM[0x40] = %u (0x%04X)\n",
           cpu.ram[0x40], cpu.ram[0x40]);
    printf("RAM[0x41] = %u\n", cpu.ram[0x41]);

    return 0;
}
