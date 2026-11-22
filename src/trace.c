/*
 * trace.c -- read-only display of the CPU while a program runs.
 *
 * For every instruction it prints:
 *   1. the three clock ticks (FETCH, DECODE, EXECUTE) with the bus and ALU
 *   2. the CPU contents: GPR, SPR, ALU, flags
 *   3. the RAM block as  address | binary | hex | ASCII
 * Part 2 (--trace-segments) shows all four complete segments; there the
 * 256 words are printed compactly as address:value, eight per line.
 * Nothing here changes the CPU; all state changes happen in cpu.c.
 */
#include <string.h>
#include "trace.h"

#define LINE 256

/* Text of the three clock ticks of the instruction being traced. */
typedef struct { char phase[3][LINE]; } TickText;

/* 16-bit word -> "0010 0000 0100 0000" */
static void word_binary(uint16_t w, char *buf)
{
    int n = 0;
    for (int b = 15; b >= 0; b--) {
        buf[n++] = (char)('0' + ((w >> b) & 1));
        if (b % 4 == 0 && b > 0)
            buf[n++] = ' ';
    }
    buf[n] = '\0';
}

/* One byte as an ASCII character, '.' if it is not printable. */
static char ascii(unsigned byte)
{
    return (byte >= 0x20 && byte < 0x7F) ? (char)byte : '.';
}

/* Describe the bus for one phase: "bus R [0x02] -> 0x7000" or "bus idle". */
static void bus_text(const Bus *bus, char *buf, size_t len)
{
    if (bus->control == 'R')
        snprintf(buf, len, "bus READ  addr=0x%02X data=0x%04X",
                 (unsigned)bus->address, (unsigned)bus->data);
    else if (bus->control == 'W')
        snprintf(buf, len, "bus WRITE addr=0x%02X data=0x%04X",
                 (unsigned)bus->address, (unsigned)bus->data);
    else
        snprintf(buf, len, "bus idle");
}

/* Clock callback from cpu.c: record what happened in this phase. */
static void on_tick(const CPU *c, Phase phase, void *ctx)
{
    TickText *t = ctx;
    char bus[64], text[48], bits[48];

    bus_text(&c->bus, bus, sizeof bus);
    if (phase == PHASE_FETCH) {
        word_binary(c->ir, bits);
        snprintf(t->phase[0], LINE,
                 "CLK %-4lu FETCH    %s | MAR=0x%02X MDR=0x%04X -> IR=%s, PC=0x%02X",
                 c->clock, bus, (unsigned)c->mar, (unsigned)c->mdr, bits, (unsigned)c->pc);
    } else if (phase == PHASE_DECODE) {
        disassemble(c->ir, text, sizeof text);
        fields_binary(c->ir, bits, sizeof bits);
        snprintf(t->phase[1], LINE, "CLK %-4lu DECODE   %s -> %s",
                 c->clock, bits, text);
    } else {
        char alu[64] = "ALU idle";
        if (c->alu.used)
            snprintf(alu, sizeof alu, "ALU 0x%04X %c 0x%04X = 0x%04X",
                     (unsigned)c->alu.a, c->alu.op, (unsigned)c->alu.b,
                     (unsigned)c->alu.out);
        snprintf(t->phase[2], LINE, "CLK %-4lu EXECUTE  %s | %s | PC=0x%02X",
                 c->clock, alu, bus, (unsigned)c->pc);
    }
}

/* True if the range is one whole segment (text, data, heap or stack). */
static int whole_segment(const TraceRange *r)
{
    return (r->first == TEXT_START  && r->last == TEXT_END)  ||
           (r->first == DATA_START  && r->last == DATA_END)  ||
           (r->first == HEAP_START  && r->last == HEAP_END)  ||
           (r->first == STACK_START && r->last == STACK_END);
}

/* Part 2 view: a whole segment, eight address:value words per line. */
static void print_segment(FILE *out, const CPU *c, const TraceRange *r)
{
    fprintf(out, "SEGMENT %s\n", segment_name((int)r->first));
    fprintf(out, "RAM words 0x%04X..0x%04X (address: value)\n", r->first, r->last);
    for (unsigned a = r->first; a <= r->last; ++a)
        fprintf(out, "%04X:%04X%s", a, (unsigned)c->ram[a],
                ((a - r->first + 1) % 8 == 0 || a == r->last) ? "\n" : " ");
}

/* Print CPU contents and the RAM ranges. */
static void print_state(FILE *out, const CPU *c,
                        const TraceRange *ranges, size_t count)
{
    char bits[24];

    /* CPU: general registers, special registers, ALU and flags */
    fprintf(out, "  GPR ");
    for (unsigned i = 0; i < NUM_REGS; ++i)
        fprintf(out, " R%u=0x%04X(%u)", i, (unsigned)c->regs[i], (unsigned)c->regs[i]);
    fprintf(out, "\n  SPR  PC=0x%04X IR=0x%04X SP=0x%04X MAR=0x%04X MDR=0x%04X\n",
            (unsigned)c->pc, (unsigned)c->ir, (unsigned)c->sp,
            (unsigned)c->mar, (unsigned)c->mdr);
    if (c->alu.used)
        fprintf(out, "  ALU  0x%04X %c 0x%04X = 0x%04X   FLAGS Z=%u N=%u\n",
                (unsigned)c->alu.a, c->alu.op, (unsigned)c->alu.b,
                (unsigned)c->alu.out, c->z, c->n);
    else
        fprintf(out, "  ALU  idle                          FLAGS Z=%u N=%u\n", c->z, c->n);

    /* RAM: whole segments compactly (Part 2), otherwise one row per word */
    int header = 0;
    for (size_t i = 0; i < count; ++i) {
        if (whole_segment(&ranges[i])) {
            print_segment(out, c, &ranges[i]);
            continue;
        }
        if (!header) {
            fprintf(out, "  RAM  addr  binary               hex   ASCII\n");
            header = 1;
        }
        for (unsigned a = ranges[i].first; a <= ranges[i].last; ++a) {
            uint16_t w = c->ram[a];
            word_binary(w, bits);
            fprintf(out, "       0x%02X  %s  %04X  |%c%c|\n", a, bits, (unsigned)w,
                    ascii(w >> 8), ascii(w & 0xFF));
        }
    }
}

/* Snapshot header, optional clock ticks, then the CPU/RAM state. */
static int print_block(FILE *out, const CPU *c, int executed, const TickText *ticks,
                       const TraceRange *ranges, size_t count)
{
    char text[48], bits[48];

    if (!out || !c || (count && !ranges))
        return -1;
    for (size_t i = 0; i < count; ++i)
        if (ranges[i].first > ranges[i].last || ranges[i].last >= MEM_SIZE)
            return -1;

    fprintf(out, "SNAPSHOT step=%lu ", c->steps);
    if (executed < 0) {
        fprintf(out, "INITIAL (loaded) -- program in RAM before execution\n");
    } else {
        disassemble(c->ir, text, sizeof text);
        fields_binary(c->ir, bits, sizeof bits);
        fprintf(out, "executed=0x%04X word=0x%04X %s   [%s]\n",
                (unsigned)executed, (unsigned)c->ir, text, bits);
    }
    if (ticks != NULL)
        for (int p = 0; p < 3; ++p)
            fprintf(out, "  %s\n", ticks->phase[p]);
    print_state(out, c, ranges, count);
    fputc('\n', out);
    return ferror(out) ? -1 : 0;
}

int trace_snapshot(FILE *out, const CPU *c, int executed,
                   const TraceRange *ranges, size_t count)
{
    return print_block(out, c, executed, NULL, ranges, count);
}

int trace_run(CPU *c, unsigned long limit, FILE *out,
              const TraceRange *ranges, size_t count)
{
    TickText ticks;
    int status = 0;

    /* Show the program as loaded into RAM */
    if (out && trace_snapshot(out, c, -1, ranges, count))
        return -1;

    /* Hook the clock so each phase is recorded */
    if (out) {
        c->on_tick = on_tick;
        c->tick_ctx = &ticks;
    }

    unsigned long executed = 0;
    while (!c->halted && !c->faulted && executed < limit) {
        unsigned address = c->pc;               /* address of this instruction */
        memset(&ticks, 0, sizeof ticks);
        if (cpu_step(c) < 0) {
            status = -1;
            break;
        }
        ++executed;
        if (out && print_block(out, c, (int)address, &ticks, ranges, count)) {
            status = -1;
            break;
        }
    }

    /* Unhook the tracer */
    c->on_tick = NULL;
    c->tick_ctx = NULL;

    if (status || c->faulted)
        return -1;
    if (!c->halted) {
        fprintf(stderr, "CPU error: Instruction limit exceeded\n");
        c->faulted = true;
        return -1;
    }
    return 0;
}