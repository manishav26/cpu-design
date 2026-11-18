/* Native reference; only factorial.asm runs on SIMPLE-16. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint16_t factorial(uint16_t n)
{
    uint16_t acc = 1;
    while (n != 0) {
        acc = (uint16_t)((uint32_t)acc * n);
        n--;
    }
    return acc;
}

int main(int argc, char **argv)
{
    unsigned long input = 8;
    if (argc > 2) return 1;
    if (argc == 2) {
        char *end;
        input = strtoul(argv[1], &end, 10);
        if (!*argv[1] || *end || input > 8) return 1;
    }
    printf("%u\n", (unsigned)factorial((uint16_t)input));
    return 0;
}
