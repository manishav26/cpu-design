/* Part 2 native reference; factorial_recursive.asm runs on SIMPLE-16. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint16_t factorial(uint16_t n)
{
    /* Each call waits for the smaller problem before multiplying its n. */
    if (n == 0) return 1;
    return (uint16_t)((uint32_t)n * factorial((uint16_t)(n - 1)));
}

int main(int argc, char **argv)
{
    unsigned long input = 4;
    if (argc > 2) return 1;
    if (argc == 2) {
        char *end;
        input = strtoul(argv[1], &end, 10);
        if (!*argv[1] || *end || input > 8) return 1;
    }
    printf("%u\n", (unsigned)factorial((uint16_t)input));
    return 0;
}
