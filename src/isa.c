/* isa.c -- SIMPLE-16 instruction table and encode / decode / disassemble.*/
#include <stdio.h>
#include <string.h>

#include "isa.h"

/*
 * Instruction table, in opcode order.
 * Columns: name, opcode, sub, format, operands, description, sets flags.
 * PUSH / POP / RET share opcode F and differ by sub.
 */
const OpInfo OPCODES[] = {
    { "HALT",  OP_HALT,  -1,       FMT_N, 0, "stop the CPU",                         0 },
    { "LDI",   OP_LDI,   -1,       FMT_I, 2, "Rd <- imm8 (0..255)",                  0 },
    { "LOAD",  OP_LOAD,  -1,       FMT_M, 2, "Rd <- M[addr7]  or  M[Rs + off7]",     0 },
    { "STORE", OP_STORE, -1,       FMT_M, 2, "M[addr7]  or  M[Rs + off7] <- Rd",     0 },
    { "MOV",   OP_MOV,   -1,       FMT_R, 2, "Rd <- Rs",                             0 },
    { "ADD",   OP_ADD,   -1,       FMT_R, 2, "Rd <- Rd + Rs",                        1 },
    { "SUB",   OP_SUB,   -1,       FMT_R, 2, "Rd <- Rd - Rs",                        1 },
    { "MUL",   OP_MUL,   -1,       FMT_R, 2, "Rd <- Rd * Rs (low 16 bits)",          1 },
    { "ADDI",  OP_ADDI,  -1,       FMT_I, 2, "Rd <- Rd + imm8 (-128..127)",          1 },
    { "CMP",   OP_CMP,   -1,       FMT_R, 2, "flags <- Rd - Rs (result dropped)",    1 },
    { "JMP",   OP_JMP,   -1,       FMT_J, 1, "PC <- addr8",                          0 },
    { "JZ",    OP_JZ,    -1,       FMT_J, 1, "if Z = 1: PC <- addr8",                0 },
    { "JNZ",   OP_JNZ,   -1,       FMT_J, 1, "if Z = 0: PC <- addr8",                0 },
    { "JN",    OP_JN,    -1,       FMT_J, 1, "if N = 1: PC <- addr8",                0 },
    { "CALL",  OP_CALL,  -1,       FMT_J, 1, "SP <- SP-1; M[SP] <- PC; PC <- addr8", 0 },
    { "PUSH",  OP_STACK, SUB_PUSH, FMT_S, 1, "SP <- SP-1; M[SP] <- Rd",              0 },
    { "POP",   OP_STACK, SUB_POP,  FMT_S, 1, "Rd <- M[SP]; SP <- SP+1",              0 },
    { "RET",   OP_STACK, SUB_RET,  FMT_S, 0, "PC <- M[SP]; SP <- SP+1",              0 },
};
const int NUM_OPCODES = (int)(sizeof OPCODES / sizeof OPCODES[0]);

/* Name -> table row (used by the assembler). NULL if unknown. */
const OpInfo *op_by_name(const char *mnemonic)
{
    for (int i = 0; i < NUM_OPCODES; i++)
        if (strcmp(OPCODES[i].name, mnemonic) == 0)
            return &OPCODES[i];
    return NULL;
}

/* Opcode (+ sub for opcode F) -> table row (used by the CPU). NULL if illegal. */
const OpInfo *op_by_code(int opcode, int sub)
{
    for (int i = 0; i < NUM_OPCODES; i++)
        if (OPCODES[i].opcode == opcode && (OPCODES[i].sub < 0 || OPCODES[i].sub == sub))
            return &OPCODES[i];
    return NULL;
}

/* Split a word into its fields: shift each field down, then mask its width. */
Fields decode(uint16_t word)
{
    Fields f;
    f.op    = (word >> 12) & 0xF;
    f.rd    = (word >> 10) & 0x3;
    f.rs    = (word >> 8)  & 0x3;
    f.imm8  =  word        & 0xFF;
    f.mode  = (word >> 7)  & 0x1;
    f.addr7 =  word        & 0x7F;
    return f;
}

/* Build a word from its fields (the reverse of decode). Unused fields stay 0. */
uint16_t encode(const OpInfo *op, int rd, int rs, int mode, int imm)
{
    unsigned w = (unsigned)(op->opcode & 0xF) << 12;
    switch (op->fmt) {
    case FMT_R:
        w |= (unsigned)(rd & 3) << 10 | (unsigned)(rs & 3) << 8;
        break;
    case FMT_I:
        w |= (unsigned)(rd & 3) << 10 | (unsigned)(imm & 0xFF);
        break;
    case FMT_M:
        w |= (unsigned)(rd & 3) << 10 | (unsigned)(rs & 3) << 8
           | (unsigned)(mode & 1) << 7 | (unsigned)(imm & 0x7F);
        break;
    case FMT_J:
        w |= (unsigned)(imm & 0xFF);
        break;
    case FMT_S:
        w |= (unsigned)(rd & 3) << 10 | (unsigned)(op->sub & 3) << 8;
        break;
    case FMT_N:
        break;
    }
    return (uint16_t)w;
}

/* 8-bit field as a signed number: 0xFF -> -1, 0x80 -> -128. */
int sign8(int value)
{
    return (value & 0x80) ? value - 256 : value;
}

const char *format_name(Format f)
{
    static const char *names[] = { "N", "R", "I", "M", "J", "S" };
    return names[f];
}

/* Address -> segment name ("text", "data", "heap", "stack"). */
const char *segment_name(int addr)
{
    if (addr <= TEXT_END)  return "text";
    if (addr <= DATA_END)  return "data";
    if (addr <= HEAP_END)  return "heap";
    return "stack";
}

/* Machine code -> assembly text */
void disassemble(uint16_t word, char *buf, size_t len)
{
    Fields f = decode(word);
    const OpInfo *op = op_by_code(f.op, f.rs);
    if (op == NULL) {
        snprintf(buf, len, "??? (illegal)");
        return;
    }
    switch (op->fmt) {
    case FMT_N: 
        snprintf(buf, len, "%s", op->name);
        break;
    case FMT_R:   
        snprintf(buf, len, "%s R%d, R%d", op->name, f.rd, f.rs);
        break;
    case FMT_J:   
        snprintf(buf, len, "%s 0x%02X", op->name, f.imm8);
        break;
    case FMT_S:   
        if (op->sub == SUB_RET) snprintf(buf, len, "%s", op->name);
        else                    snprintf(buf, len, "%s R%d", op->name, f.rd);
        break;
    case FMT_I:   
        if (f.op == OP_ADDI) snprintf(buf, len, "%s R%d, %d", op->name, f.rd, sign8(f.imm8));
        else                 snprintf(buf, len, "%s R%d, %d", op->name, f.rd, f.imm8);
        break;
    case FMT_M:  
        if (f.mode == 0)       snprintf(buf, len, "%s R%d, [0x%02X]", op->name, f.rd, f.addr7);
        else if (f.addr7 == 0) snprintf(buf, len, "%s R%d, [R%d]", op->name, f.rd, f.rs);
        else                   snprintf(buf, len, "%s R%d, [R%d+%d]", op->name, f.rd, f.rs, f.addr7);
        break;
    }
}

/* Append `bits` bits of value to buf as '0'/'1'. */
static void put_bits(char *buf, size_t len, unsigned value, int bits)
{
    size_t n = strlen(buf);
    for (int b = bits - 1; b >= 0 && n + 1 < len; b--)
        buf[n++] = (char)('0' + ((value >> b) & 1));
    buf[n] = '\0';
}

/* Append a string to buf. */
static void put_text(char *buf, size_t len, const char *s)
{
    size_t n = strlen(buf);
    snprintf(buf + n, len - n, "%s", s);
}
/* Machine code -> binary fields */
void fields_binary(uint16_t word, char *buf, size_t len)
{
    Fields f = decode(word);
    const OpInfo *op = op_by_code(f.op, f.rs);
    buf[0] = '\0';
    put_bits(buf, len, (unsigned)f.op, 4);
    put_text(buf, len, " ");
    put_bits(buf, len, (unsigned)f.rd, 2);
    put_text(buf, len, " ");
    put_bits(buf, len, (unsigned)f.rs, 2);
    put_text(buf, len, " ");
    if (op != NULL && op->fmt == FMT_M) {          /* LOAD/STORE: M bit + addr7 */
        put_bits(buf, len, (unsigned)f.mode, 1);
        put_text(buf, len, " ");
        put_bits(buf, len, (unsigned)f.addr7, 7);
    } else {
        put_bits(buf, len, (unsigned)f.imm8, 8);
    }
}