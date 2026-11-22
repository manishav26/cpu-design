/*
 * main.c -- SIMPLE-16 command line.
 *
 *   simple16 program.asm            run, print the result summary
 *   simple16 --trace   program.asm  run, print RAM after loading, then the
 *                                   FETCH/DECODE/EXECUTE clock ticks, CPU
 *                                   and RAM after every instruction
 *   simple16 --trace-segments program.asm
 *                                   same, but RAM shown as all four segments
 *                                   (text, data, heap, stack) -- Part 2
 *   simple16 --listing program.asm  print the assembled words (no run)
 *   simple16 --isa                  print the instruction set table
 */
#include <stdio.h>
#include <string.h>
#include "assembler.h"
#include "loader.h"
#include "trace.h"

#define MAX_STEPS 10000

/* Bit pattern of an instruction format: d = Rd, s = Rs, m = mode, a = address, i = immediate. */
static const char *pattern(const OpInfo *op)
{
    switch (op->fmt) {
    case FMT_R: return "dd ss 0000 0000";
    case FMT_I: return "dd 00 iiii iiii";
    case FMT_M: return "dd ss m aaa aaaa";
    case FMT_J: return "00 00 aaaa aaaa";
    case FMT_S: return op->sub == SUB_PUSH ? "dd 00 0000 0000"
                     : op->sub == SUB_POP  ? "dd 01 0000 0000"
                     :                       "00 10 0000 0000";
    default:    return "00 00 0000 0000";
    }
}

/* --isa: one row per instruction with its binary code. */
static void print_isa(void)
{
    puts("Instr  Opcode  Fmt  Binary code (16 bits)   Operation");
    for (int i = 0; i < NUM_OPCODES; ++i) {
        const OpInfo *op = &OPCODES[i];
        char bits[5];
        for (int b = 0; b < 4; ++b)
            bits[b] = (char)('0' + ((op->opcode >> (3 - b)) & 1));
        bits[4] = '\0';
        printf("%-6s %-7s %-4s %s %-17s %s\n", op->name, bits,
               format_name(op->fmt), bits, pattern(op), op->desc);
    }
}

/* --listing: address, word, binary fields and source line. */
static void print_listing(const Program *p)
{
    puts("Address Word Fields | Label | Source");
    for (int i = 0; i < p->count; ++i) {
        char bits[128];
        fields_binary(p->listing[i].word, bits, sizeof bits);
        printf("%04X %04X %s | %s | %s\n", p->listing[i].addr,
               (unsigned)p->listing[i].word, bits, p->listing[i].label, p->listing[i].text);
    }
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--isa")) {
        print_isa();
        return 0;
    }
    int tracing = argc == 3 && !strcmp(argv[1], "--trace");
    int segments = argc == 3 && !strcmp(argv[1], "--trace-segments");
    int listing = argc == 3 && !strcmp(argv[1], "--listing");
    if (argc != 2 && !tracing && !segments && !listing) {
        fprintf(stderr, "Usage: %s [--trace|--trace-segments|--listing] program.asm\n"
                        "       %s --isa\n", argv[0], argv[0]);
        return 1;
    }

    /* Assemble the source into a memory image */
    static Program p;
    CPU cpu;
    char error[256];
    if (assemble_file(argv[argc - 1], &p, error, sizeof error)) {
        fprintf(stderr, "Assembler error: %s\n", error);
        return 1;
    }
    if (listing) {
        print_listing(&p);
        return 0;
    }

    /* Load the image into RAM */
    if (cpu_load_program(&cpu, &p)) {
        fprintf(stderr, "Failed to load program.\n");
        return 1;
    }

    /* RAM block to show: the code words, then the data words */
    int last_text = -1, last_data = -1;
    for (int i = 0; i < p.count; ++i) {
        int a = p.listing[i].addr;
        if (a <= TEXT_END && a > last_text) last_text = a;
        if (a >= DATA_START && a <= DATA_END && a > last_data) last_data = a;
    }
    TraceRange ranges[2];
    size_t count = 0;
    if (last_text >= 0) ranges[count++] = (TraceRange){ TEXT_START, (unsigned)last_text };
    if (last_data >= 0) ranges[count++] = (TraceRange){ DATA_START, (unsigned)last_data };

    /* Part 2: all 256 words in four named segments */
    static const TraceRange segment_ranges[] = {
        { TEXT_START, TEXT_END }, { DATA_START, DATA_END },
        { HEAP_START, HEAP_END }, { STACK_START, STACK_END }
    };

    /* Run: same engine for normal and both trace modes */
    if (trace_run(&cpu, MAX_STEPS, (tracing || segments) ? stdout : NULL,
                  segments ? segment_ranges : ranges, segments ? 4 : count)) {
        fprintf(stderr, "CPU execution failed.\n");
        return 1;
    }

    /* Result summary */
    printf("HALT steps=%lu clock=%lu R0=%u (0x%04X)\n", cpu.steps, cpu.clock,
           (unsigned)cpu.regs[0], (unsigned)cpu.regs[0]);
    for (int i = 0; i < p.count; ++i)
        if (p.listing[i].is_data)
            printf("%s RAM[0x%04X]=%u (0x%04X)\n", p.listing[i].label,
                   p.listing[i].addr, (unsigned)cpu.ram[p.listing[i].addr],
                   (unsigned)cpu.ram[p.listing[i].addr]);
    return 0;
}