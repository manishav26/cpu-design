/*
 * isa.h -- SIMPLE-16 instruction set (shared by assembler, loader, CPU)
 *
 * 16-bit words, 4-bit opcode (16 instructions), registers R0-R3,
 * 256 words of memory: text 0x00-3F, data 0x40-7F, heap 0x80-BF, stack 0xC0-FF
 *
 *   R  [ op 4 | Rd 2 | Rs 2  | 0000 0000     ]   MOV ADD SUB MUL CMP
 *   I  [ op 4 | Rd 2 | 00    | imm 8         ]   LDI ADDI
 *   M  [ op 4 | Rd 2 | Rs 2  | M 1 | addr 7  ]   LOAD STORE
 *   J  [ op 4 | 00   | 00    | addr 8        ]   JMP JZ JNZ JN CALL
 *   S  [ op 4 | Rd 2 | sub 2 | 0000 0000     ]   PUSH POP RET
 *   N  [ op 4 | 0000 0000 0000               ]   HALT
 */
#ifndef ISA_H
#define ISA_H
#include <stddef.h>
#include <stdint.h>
#define MEM_SIZE     256        /* words of RAM */
#define NUM_REGS     4          /* R0..R3 */

/* Memory map (segments) */
#define TEXT_START   0x00       /* code */
#define TEXT_END     0x3F
#define DATA_START   0x40       /* global variables */
#define DATA_END     0x7F
#define HEAP_START   0x80       /* dynamic data (reached through pointers) */
#define HEAP_END     0xBF
#define STACK_START  0xC0       /* stack: grows DOWN from 0xFF towards 0xC0*/
#define STACK_END    0xFF
#define SP_START     0x100      /* empty stack: the first PUSH writes 0xFF */
#define DIRECT_MAX   0x7F       /* largest address reachable with M = 0 */

/* Instruction formats */
typedef enum { FMT_N, FMT_R, FMT_I, FMT_M, FMT_J, FMT_S } Format;

/* Opcodes (bits 15..12) */
enum {
    OP_HALT = 0x0, OP_LDI = 0x1, OP_LOAD = 0x2, OP_STORE = 0x3,
    OP_MOV  = 0x4, OP_ADD = 0x5, OP_SUB  = 0x6, OP_MUL   = 0x7,
    OP_ADDI = 0x8, OP_CMP = 0x9, OP_JMP  = 0xA, OP_JZ    = 0xB,
    OP_JNZ  = 0xC, OP_JN  = 0xD, OP_CALL = 0xE, OP_STACK = 0xF
};

/* Sub-operations of opcode F (bits 9..8) */
enum { SUB_PUSH = 0, SUB_POP = 1, SUB_RET = 2 };

/* One row of the instruction table */
typedef struct {
    const char *name;     /* mnemonic, e.g. "ADD" */
    int         opcode;   /* 4-bit opcode */
    int         sub;      /* sub-operation for opcode F, otherwise -1 */
    Format      fmt;      /* instruction format */
    int         nops;     /* number of operands in assembly */
    const char *desc;     /* register-transfer description */
    int         flags;    /* 1 if the instruction updates Z and N */
} OpInfo;

extern const OpInfo OPCODES[];
extern const int    NUM_OPCODES;

const OpInfo *op_by_name(const char *mnemonic);   /* NULL if unknown */
const OpInfo *op_by_code(int opcode, int sub);    /* NULL if illegal */

typedef struct {
    int op;      /* bits 15..12  */
    int rd;      /* bits 11..10  */
    int rs;      /* bits  9..8  (also the F sub-op) */
    int imm8;    /* bits  7..0  */
    int mode;    /* bit   7 (M-type) */
    int addr7;   /* bits  6..0 (M-type) */
} Fields;

Fields   decode(uint16_t word);
uint16_t encode(const OpInfo *op, int rd, int rs, int mode, int imm);

int  sign8(int value);
void disassemble(uint16_t word, char *buf, size_t len);
void fields_binary(uint16_t word, char *buf, size_t len);
const char *format_name(Format f);
const char *segment_name(int addr);

#endif 