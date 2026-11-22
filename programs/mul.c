/* mul.c -- multiply two numbers. Translated by hand into mul.asm. */
#include <stdint.h>
#include <stdio.h>

uint16_t a = 6;         /* data segment 0x40 */
uint16_t b = 7;         /* data segment 0x41 */
uint16_t result;        /* data segment 0x42 */

int main(void)
{
    result = (uint16_t)(a * b);
    printf("%u\n", (unsigned)result);       /* 42 */
    return 0;
}