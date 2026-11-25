/*
 * recursive.c -- Part 2 checks.
 *
 * For n = 0, 1, 4, 8 runs the recursive factorial and checks:
 *   - the result agrees with the native C program
 *   - every snapshot prints all four segments and all 256 RAM words
 *   - the heap stays empty (no dynamic allocation)
 *   - each CALL saves its return address (and the parent's n) on the stack
 *   - one RET per CALL, the stack depth is 2 words per call,
 *     and SP is back at 0x100 at the end
 */
#include "run.h"

#define MAX_BLOCKS 256

/* Value of "NAME=0x...." in a block, e.g. field(block, "SP=0x"). */
static unsigned field(const char *block, const char *name)
{
    const char *at = strstr(block, name);
    assert(at);
    return (unsigned)strtoul(at + strlen(name), NULL, 16);
}

/* Parse every "AAAA:VVVV" pair in a block into ram[]; return how many. */
static int parse_ram(const char *block, unsigned ram[256])
{
    int count = 0;
    for (const char *p = block; *p; ++p) {
        unsigned a, v;
        int used = 0;
        if (p[1] && p[2] && p[3] && p[4] == ':' && sscanf(p, "%4x:%4x%n", &a, &v, &used) == 2 && used == 9
            && (p == block || p[-1] == ' ' || p[-1] == '\n')) {
            assert(a == (unsigned)count && a < 256);   /* addresses in order */
            ram[a] = v;
            ++count;
            p += 8;
        }
    }
    return count;
}

int main(void)
{
    char *source = read_file("programs/factorial_recursive.asm");
    const int inputs[] = { 0, 1, 4, 8 };

    for (int t = 0; t < 4; ++t) {
        int n = inputs[t];
        char cmd[128], path[64], word[32], want[64];

        snprintf(word, sizeof word, "input: .word %d", n);
        char *changed = replace(source, "input: .word 4", word);
        write_temp(path, changed);
        snprintf(cmd, sizeof cmd, "./build/factorial_recursive %d", n);
        char *ref = run(cmd, NULL);
        snprintf(cmd, sizeof cmd, "./build/simple16 %s", path);
        char *normal = run(cmd, NULL);
        snprintf(cmd, sizeof cmd, "./build/simple16 --trace-segments %s", path);
        char *trace = run(cmd, NULL);
        remove(path);

        /* the traced run ends with the same summary as a normal run */
        size_t tl = strlen(trace), nl = strlen(normal);
        assert(tl >= nl && !strcmp(trace + tl - nl, normal));
        snprintf(want, sizeof want, "result RAM[0x0041]=%ld ", strtol(ref, NULL, 10));
        assert(strstr(normal, want));

        char *blocks[MAX_BLOCKS];
        int count = split_snapshots(trace, blocks, MAX_BLOCKS);
        assert(count == 9 * n + 10);                 /* initial + 9n+9 steps */
        assert(strstr(blocks[0], "INITIAL") && strstr(blocks[count - 1], "HALT"));

        int calls = 0, returns = 0;
        unsigned min_sp = 0x100, last_sp = 0;
        for (int i = 0; i < count; ++i) {
            const char *b = blocks[i];
            /* four segments, in order */
            const char *s1 = strstr(b, "\nSEGMENT text\n"), *s2 = strstr(b, "\nSEGMENT data\n");
            const char *s3 = strstr(b, "\nSEGMENT heap\n"), *s4 = strstr(b, "\nSEGMENT stack\n");
            assert(s1 && s2 && s3 && s4 && s1 < s2 && s2 < s3 && s3 < s4);

            unsigned ram[256];
            assert(parse_ram(b, ram) == 256);        /* all 256 words shown */
            for (int a = 0x80; a < 0xC0; ++a)
                assert(ram[a] == 0);                 /* heap unused */

            unsigned sp = field(b, "SP=0x");
            if (sp < min_sp) min_sp = sp;
            last_sp = sp;

            if (strstr(b, " CALL ")) {
                ++calls;
                assert(strstr(b, "PC=0x0004"));
                unsigned executed = field(b, "executed=0x");
                assert(ram[sp] == executed + 1);     /* return address saved */
                if (calls > 1)
                    assert((int)ram[sp + 1] == n - calls + 2);   /* parent's n */
            }
            if (strstr(b, " RET"))
                ++returns;
        }
        assert(calls == n + 1 && returns == n + 1);
        assert(min_sp == (unsigned)(0xFF - 2 * n));
        assert(last_sp == 0x100);

        free(changed); free(ref); free(normal); free(trace);
    }
    free(source);
    puts("PASS recursive factorial 0, 1, 4, 8; call frames and all four segments");
    return 0;
}