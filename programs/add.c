/* add.c -- add two numbers. Translated by hand into add.asm. */
#include <stdint.h>
#include <stdio.h>

uint16_t a = 25;        /* data segment 0x40 */
uint16_t b = 17;        /* data segment 0x41 */
uint16_t result;        /* data segment 0x42 */

int main(void)
{
    result = (uint16_t)(a + b);
    printf("%u\n", (unsigned)result);       /* 42 */
    return 0;
}