/* Execution integration for normal, listing, and trace output. */
#include <stdio.h>
#include <string.h>
#include "assembler.h"
#include "loader.h"
#include "trace.h"

int main(int argc, char **argv)
{
    int tracing = argc == 3 && !strcmp(argv[1], "--trace");
    int listing = argc == 3 && !strcmp(argv[1], "--listing");
    if (argc != 2 && !tracing && !listing) {
        fprintf(stderr, "Usage: %s [--trace|--listing] program.asm\n", argv[0]);
        return 1;
    }
    static Program p;
    CPU cpu;
    char error[256];
    if (assemble_file(argv[argc-1], &p, error, sizeof error)) {
        fprintf(stderr, "Assembler error: %s\n", error);
        return 1;
    }
    if (listing) {
        puts("Address Word Fields | Label | Source");
        for (int i = 0; i < p.count; ++i) {
            char bits[128];
            fields_binary(p.listing[i].word, bits, sizeof bits);
            printf("%04X %04X %s | %s | %s\n", p.listing[i].addr,
                   (unsigned)p.listing[i].word, bits, p.listing[i].label, p.listing[i].text);
        }
        return 0;
    }
    if (cpu_load_program(&cpu, &p)) return 1;
    unsigned last = 0;
    for (int i = 0; i < p.count; ++i)
        if ((unsigned)p.listing[i].addr > last) last = (unsigned)p.listing[i].addr;
    TraceRange range = {0, last};
    if (trace_run(&cpu, 10000, tracing ? stdout : NULL, &range, 1)) {
        fprintf(stderr, "CPU execution failed.\n");
        return 1;
    }
    printf("HALT steps=%lu R0=%u (0x%04X)\n", cpu.steps,
           (unsigned)cpu.regs[0], (unsigned)cpu.regs[0]);
    for (int i = 0; i < p.count; ++i)
        if (p.listing[i].is_data)
            printf("%s RAM[0x%04X]=%u (0x%04X)\n", p.listing[i].label,
                   p.listing[i].addr, (unsigned)cpu.ram[p.listing[i].addr],
                   (unsigned)cpu.ram[p.listing[i].addr]);
    return 0;
}
