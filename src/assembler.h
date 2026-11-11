/*
 * assembler.h -- two-pass assembler for SIMPLE-16.
 * Syntax:  label:  MNEMONIC operands; comment(.text, .data, .word)
 */
#ifndef ASSEMBLER_H
#define ASSEMBLER_H
#include <stddef.h>
#include <stdint.h>
#include "isa.h"
#define MAX_LINE   256
#define MAX_LABEL  32
/* One assembled word */
typedef struct {
    int      addr;
    uint16_t word;
    char     label[MAX_LABEL];
    char     text[MAX_LINE];
    char     comment[MAX_LINE];
    int      is_data;               /* 1 = .word, 0 = instruction */
} ListingEntry;
/* Assembled program: memory image + listing */
typedef struct {
    uint16_t     mem[MEM_SIZE];
    ListingEntry listing[MEM_SIZE];
    int          count;
} Program;
/* Returns 0, or -1 with an error message in err. */
int assemble_file(const char *path, Program *prog, char *err, size_t errlen);
#endif