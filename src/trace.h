/*
 * trace.h -- read-only display of CPU and RAM while a program runs.
 */
#ifndef TRACE_H
#define TRACE_H
#include <stdio.h>
#include "cpu.h"

/* Inclusive word-address range of RAM to display (e.g. text, data). */
typedef struct { unsigned first, last; } TraceRange;

/* Print one snapshot: CPU registers, ALU, flags and the RAM ranges
 * (binary | hex | ASCII). executed_address < 0 means "just loaded". */
int trace_snapshot(FILE *out, const CPU *cpu, int executed_address,
                   const TraceRange *ranges, size_t count);

/* Run with the same cpu_step engine. If out != NULL, print the loaded
 * RAM, then for every instruction its FETCH / DECODE / EXECUTE clock
 * ticks and a snapshot. limit = maximum instructions. */
int trace_run(CPU *cpu, unsigned long limit, FILE *out,
              const TraceRange *ranges, size_t count);

#endif