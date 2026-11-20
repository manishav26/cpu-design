#include "trace.h"

/* Format a loaded or post-instruction state without changing registers or RAM.
 * executed < 0 identifies the initial state, when IR has not been fetched yet. */

int trace_snapshot(FILE *out, const CPU *c, int executed,
                   const TraceRange *ranges, size_t count)
{
    char decoded[128];
    if (!out || !c || (count && !ranges)) return -1;
    /* Validate every range before printing or indexing RAM. Endpoints are inclusive. */
    for (size_t i = 0; i < count; ++i)
        if (ranges[i].first > ranges[i].last || ranges[i].last >= MEM_SIZE)
            return -1;
    fprintf(out, "SNAPSHOT step=%lu ", c->steps);
    if (executed < 0) fprintf(out, "INITIAL (loaded)\n");
    else {
        /* IR is the word actually fetched, even if STORE overwrote its RAM cell. */
        disassemble(c->ir, decoded, sizeof decoded);
        fprintf(out, "executed=0x%04X word=0x%04X %s\n",
                (unsigned)executed, (unsigned)c->ir, decoded);
    }
    /* Hex shows bit patterns; unsigned decimal makes arithmetic results readable. */
    for (unsigned i = 0; i < NUM_REGS; ++i)
        fprintf(out, "R%u=0x%04X(%u) ", i, (unsigned)c->regs[i], (unsigned)c->regs[i]);
    fprintf(out, "\nPC=0x%04X IR=0x%04X FLAGS Z=%u N=%u SP=0x%04X MAR=0x%04X MDR=0x%04X\n",
            (unsigned)c->pc, (unsigned)c->ir, c->z, c->n,
            (unsigned)c->sp, (unsigned)c->mar, (unsigned)c->mdr);
    /* Repeat the complete selected blocks, including unchanged and zero words.
     * Eight address:value pairs per line keep word addresses easy to follow. */
    for (size_t i = 0; i < count; ++i) {
        /* Name complete architectural segments; arbitrary selected blocks retain
         * the plain RAM heading used by the compact Part 1 trace. */
        if ((ranges[i].first == TEXT_START && ranges[i].last == TEXT_END) ||
            (ranges[i].first == DATA_START && ranges[i].last == DATA_END) ||
            (ranges[i].first == HEAP_START && ranges[i].last == HEAP_END) ||
            (ranges[i].first == STACK_START && ranges[i].last == STACK_END))
            fprintf(out, "SEGMENT %s\n", segment_name((int)ranges[i].first));
        fprintf(out, "RAM words 0x%04X..0x%04X (address: value)\n", ranges[i].first, ranges[i].last);
        for (unsigned a = ranges[i].first; a <= ranges[i].last; ++a) {
            fprintf(out, "%04X:%04X%s", a, (unsigned)c->ram[a],
                    ((a-ranges[i].first+1)%8 == 0 || a == ranges[i].last) ? "\n" : " ");
        }
    }
    fputc('\n', out);
    return ferror(out) ? -1 : 0;
}

/* Both CLI modes use the same CPU engine. A NULL stream disables formatting;
 * the instruction budget counts steps taken by this invocation. */
int trace_run(CPU *c, unsigned long limit, FILE *out,
              const TraceRange *ranges, size_t count)
{
    /* Loading happens in the caller, so this snapshot precedes the first fetch. */
    if (out && trace_snapshot(out, c, -1, ranges, count)) return -1;
    unsigned long executed = 0;
    while (!c->halted && !c->faulted && executed < limit) {
        /* Save PC before stepping: branches and CALL/RET can replace the next PC. */
        unsigned address = c->pc;
        /* Failed instructions produce an error, not a successful-step snapshot. */
        if (cpu_step(c) < 0) return -1;
        ++executed;
        if (out && trace_snapshot(out, c, (int)address, ranges, count)) return -1;
    }
    if (c->faulted) return -1;
    /* Exhausting the budget is an error; it must not look like a normal HALT. */
    if (!c->halted) {
        fprintf(stderr, "CPU error: Instruction limit exceeded\n");
        c->faulted = true;
        return -1;
    }
    return 0;
}
