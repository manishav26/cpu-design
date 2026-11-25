/*
 * cli.c -- black-box checks of the simulator binary.
 *
 * Runs ./build/simple16 exactly as a user would and checks:
 *   1. factorial agrees with the native C program for n = 0, 1, 5, 8
 *   2. every trace snapshot shows the code and data words as
 *      binary | hex | ASCII, and every executed instruction shows
 *      its three clock ticks (FETCH, DECODE, EXECUTE)
 *   3. add and mul agree with their native C programs
 *   4. bad command lines exit with an error
 *   5. regressions for the CPU and assembler fixes
 */
#include "run.h"

#define MAX_BLOCKS 256

/* Check one program's --trace output. */
static void check_trace(const char *name, int steps, int code, int data)
{
    char cmd[128];
    snprintf(cmd, sizeof cmd, "./build/simple16 --trace programs/%s.asm", name);
    char *trace = run(cmd, NULL);
    char *blocks[MAX_BLOCKS];
    int count = split_snapshots(trace, blocks, MAX_BLOCKS);
    assert(count == steps + 1);                      /* initial + one per step */

    for (int i = 0; i < count; ++i) {
        /* expected RAM rows: code words 0x00.., then data words 0x40.. */
        int expected[64], n_expected = 0, n_rows = 0;
        for (int a = 0; a < code; ++a) expected[n_expected++] = a;
        for (int a = 0; a < data; ++a) expected[n_expected++] = 0x40 + a;

        char *copy = strdup(blocks[i]);
        for (char *line = strtok(copy, "\n"); line; line = strtok(NULL, "\n")) {
            while (*line == ' ') ++line;
            if (strncmp(line, "0x", 2) != 0) continue;
            unsigned addr, hex;
            char b[4][8], ascii[8];
            assert(sscanf(line, "%x %4s %4s %4s %4s %x %7s",
                          &addr, b[0], b[1], b[2], b[3], &hex, ascii) == 7);
            assert(n_rows < n_expected && (int)addr == expected[n_rows]);
            char bits[40];
            snprintf(bits, sizeof bits, "%s%s%s%s", b[0], b[1], b[2], b[3]);
            assert(strtoul(bits, NULL, 2) == hex);   /* binary matches hex */
            assert(ascii[0] == '|');                 /* ASCII column shown */
            ++n_rows;
        }
        assert(n_rows == n_expected);
        free(copy);

        if (i > 0)                                   /* three clock ticks */
            assert(strstr(blocks[i], " FETCH ") && strstr(blocks[i], " DECODE ")
                   && strstr(blocks[i], " EXECUTE "));
    }
    assert(strstr(blocks[count - 1], "executed=0x") && strstr(blocks[count - 1], "HALT"));

    if (!strcmp(name, "factorial")) {                /* loop jumps back */
        int found = 0;
        for (int i = 0; i < count; ++i)
            if (strstr(blocks[i], "executed=0x0007")) {
                assert(strstr(blocks[i], "JMP") && strstr(blocks[i], "PC=0x0003"));
                found = 1;
                break;
            }
        assert(found);
    }
    free(trace);
}

/* Run a program that must fail; its stderr must contain `error`. */
static void expect_error(const char *source, const char *error)
{
    char path[64], cmd[128];
    int status;
    write_temp(path, source);
    snprintf(cmd, sizeof cmd, "./build/simple16 %s 2>&1", path);
    char *out = run(cmd, &status);
    assert(status != 0 && strstr(out, error));
    free(out);
    remove(path);
}

int main(void)
{
    char cmd[128], path[64], want[64];
    int status;

    /* 1. factorial agrees with native C for several inputs */
    char *source = read_file("programs/factorial.asm");
    const int inputs[] = { 0, 1, 5, 8 };
    for (int i = 0; i < 4; ++i) {
        snprintf(cmd, sizeof cmd, "./build/factorial %d", inputs[i]);
        char *ref = run(cmd, NULL);
        char word[32];
        snprintf(word, sizeof word, "input: .word %d", inputs[i]);
        char *changed = replace(source, "input: .word 8", word);
        write_temp(path, changed);
        snprintf(cmd, sizeof cmd, "./build/simple16 %s", path);
        char *out = run(cmd, NULL);
        snprintf(want, sizeof want, "result RAM[0x0041]=%ld ", strtol(ref, NULL, 10));
        assert(strstr(out, want));
        remove(path);
        free(ref); free(changed); free(out);
    }
    free(source);

    /* 2. traces: RAM rows and clock phases */
    check_trace("square", 4, 4, 2);
    check_trace("factorial", 47, 10, 2);
    check_trace("add", 5, 5, 3);
    check_trace("mul", 5, 5, 3);

    /* 3. add and mul agree with native C */
    const char *names[] = { "add", "mul" };
    for (int i = 0; i < 2; ++i) {
        snprintf(cmd, sizeof cmd, "./build/%s", names[i]);
        char *ref = run(cmd, NULL);
        snprintf(cmd, sizeof cmd, "./build/simple16 programs/%s.asm", names[i]);
        char *out = run(cmd, NULL);
        snprintf(want, sizeof want, "result RAM[0x0042]=%ld ", strtol(ref, NULL, 10));
        assert(strstr(out, want));
        free(ref); free(out);
    }
    char *isa = run("./build/simple16 --isa", NULL);
    assert(strstr(isa, "MUL    0111"));
    free(isa);

    /* 4. bad command lines fail */
    const char *bad[] = { "", "--bogus programs/square.asm", "missing.asm" };
    for (int i = 0; i < 3; ++i) {
        snprintf(cmd, sizeof cmd, "./build/simple16 %s >/dev/null 2>&1", bad[i]);
        free(run(cmd, &status));
        assert(status != 0);
    }
    puts("PASS native C agreement, RAM snapshots, clock phases, CLI errors");

    /* 5. regressions for the CPU and assembler fixes */
    write_temp(path, ".text\nLOAD R0, [large]\nMUL R0, R0\nHALT\n.data\nlarge: .word 65535\n");
    snprintf(cmd, sizeof cmd, "./build/simple16 %s", path);
    char *out = run(cmd, NULL);
    assert(strstr(out, "R0=1 (0x0001)"));            /* 65535^2 low 16 bits */
    free(out);
    remove(path);
    expect_error(".text\nLOAD R0, [large]\nLOAD R1, [R0+65]\nHALT\n.data\nlarge: .word 65535\n",
                 "LOAD address out of range");
    expect_error(".text\nLOAD R0, [large]\nSTORE R1, [R0+65]\nHALT\n.data\nlarge: .word 65535\n",
                 "STORE address out of range");
    expect_error(".text\nJMP 64\n", "outside the text segment");
    expect_error(".text\nCALL 64\n", "outside the text segment");
    expect_error(".text\nHALT\n.data\nx: .word 65536\n", "does not fit in 16 bits");
    expect_error(".text\nHALT\n.data\nx: .word -32769\n", "does not fit in 16 bits");
    puts("PASS merged CPU and assembler fix regressions");
    return 0;
}