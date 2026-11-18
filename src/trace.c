#include "trace.h"

int trace_snapshot(FILE *out, const CPU *c, int executed,
                   const TraceRange *ranges, size_t count)
{
    char decoded[128];
    if (!out || !c || (count && !ranges)) return -1;
    for (size_t i = 0; i < count; ++i)
        if (ranges[i].first > ranges[i].last || ranges[i].last >= MEM_SIZE)
            return -1;
    fprintf(out, "SNAPSHOT step=%lu ", c->steps);
    if (executed < 0) fprintf(out, "INITIAL (loaded)\n");
    else {
        disassemble(c->ir, decoded, sizeof decoded);
        fprintf(out, "executed=0x%04X word=0x%04X %s\n",
                (unsigned)executed, (unsigned)c->ir, decoded);
    }
    for (unsigned i = 0; i < NUM_REGS; ++i)
        fprintf(out, "R%u=0x%04X(%u) ", i, (unsigned)c->regs[i], (unsigned)c->regs[i]);
    fprintf(out, "\nPC=0x%04X IR=0x%04X FLAGS Z=%u N=%u SP=0x%04X MAR=0x%04X MDR=0x%04X\n",
            (unsigned)c->pc, (unsigned)c->ir, c->z, c->n,
            (unsigned)c->sp, (unsigned)c->mar, (unsigned)c->mdr);
    for (size_t i = 0; i < count; ++i) {
        fprintf(out, "RAM words 0x%04X..0x%04X (address: value)\n", ranges[i].first, ranges[i].last);
        for (unsigned a = ranges[i].first; a <= ranges[i].last; ++a) {
            fprintf(out, "%04X:%04X%s", a, (unsigned)c->ram[a],
                    ((a-ranges[i].first+1)%8 == 0 || a == ranges[i].last) ? "\n" : " ");
        }
    }
    fputc('\n', out);
    return ferror(out) ? -1 : 0;
}

int trace_run(CPU *c, unsigned long limit, FILE *out,
              const TraceRange *ranges, size_t count)
{
    if (out && trace_snapshot(out, c, -1, ranges, count)) return -1;
    unsigned long executed = 0;
    while (!c->halted && !c->faulted && executed < limit) {
        unsigned address = c->pc;
        if (cpu_step(c) < 0) return -1;
        ++executed;
        if (out && trace_snapshot(out, c, (int)address, ranges, count)) return -1;
    }
    if (c->faulted) return -1;
    if (!c->halted) {
        fprintf(stderr, "CPU error: Instruction limit exceeded\n");
        c->faulted = true;
        return -1;
    }
    return 0;
}
