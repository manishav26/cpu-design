#ifndef TRACE_H
#define TRACE_H
#include <stdio.h>
#include "cpu.h"
/* Inclusive word-address range. Multiple ranges support four-segment views. */
typedef struct { unsigned first, last; } TraceRange;
int trace_snapshot(FILE *out, const CPU *cpu, int executed_address,
                   const TraceRange *ranges, size_t count);
/* Same cpu_step engine with optional read-only snapshots; limit is per run. */
int trace_run(CPU *cpu, unsigned long limit, FILE *out,
              const TraceRange *ranges, size_t count);
#endif
