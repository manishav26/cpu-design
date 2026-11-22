/*
 * cpu.c -- SIMPLE-16 CPU execution engine
 *
 * Each instruction takes three clock ticks:
 *   FETCH    MAR <- PC, bus read, MDR <- RAM[MAR], IR <- MDR, PC <- PC + 1
 *   DECODE   split IR into opcode / register / address fields
 *   EXECUTE  ALU, bus read/write, or PC update, depending on the opcode
 * Instruction encoding follows isa.h / isa.c.
 */

#include <stdio.h>
#include <string.h>
#include "cpu.h"

/* Update Zero and Negative flags from a 16-bit result. */
static void update_flags(CPU *cpu, uint16_t result)
{
    cpu->z = (result == 0);
    cpu->n = ((result & 0x8000u) != 0);
}

/* ALU: compute a (op) b, keep the inputs for the trace, set Z and N. */
static uint16_t alu(CPU *cpu, char op, uint16_t a, uint16_t b)
{
    uint16_t out;
    if (op == '+')
        out = (uint16_t)(a + b);
    else if (op == '-')
        out = (uint16_t)(a - b);
    else                                        /* '*': keep low 16 bits */
        out = (uint16_t)((uint32_t)a * (uint32_t)b);
    cpu->alu = (Alu){ true, op, a, b, out };
    update_flags(cpu, out);
    return out;
}

/* Bus read: MAR drives the address lines, RAM puts the word on the data lines. */
static uint16_t read_memory(CPU *cpu, uint16_t address)
{
    cpu->mar = address;
    cpu->mdr = cpu->ram[address];
    cpu->bus = (Bus){ cpu->mar, cpu->mdr, 'R' };
    return cpu->mdr;
}

/* Bus write: MAR drives the address lines, MDR drives the data lines. */
static void write_memory(CPU *cpu, uint16_t address,
                         uint16_t value)
{
    cpu->mar = address;
    cpu->mdr = value;
    cpu->bus = (Bus){ cpu->mar, cpu->mdr, 'W' };
    cpu->ram[address] = value;
}

/* End of a clock phase: count the tick and let the tracer look. */
static void tick(CPU *cpu, Phase phase)
{
    cpu->clock++;
    if (cpu->on_tick != NULL)
        cpu->on_tick(cpu, phase, cpu->tick_ctx);
    cpu->bus.control = '-';                     /* bus idle until next transfer */
}

/* Mark execution as failed. */
static int cpu_error(CPU *cpu, const char *message)
{
    fprintf(stderr, "CPU error: %s\n", message);
    cpu->faulted = true;
    return -1;
}

/* Initialize CPU state and RAM. */
void cpu_reset(CPU *cpu)
{
    memset(cpu, 0, sizeof(*cpu));
    cpu->sp = SP_START;
    cpu->bus.control = '-';
}

/* FETCH: read the instruction at PC over the bus into IR, then PC + 1. */
static int fetch(CPU *cpu)
{
    if (cpu->pc > TEXT_END)
        return cpu_error(cpu, "PC outside text segment");

    cpu->alu.used = false;
    cpu->ir = read_memory(cpu, cpu->pc);
    cpu->pc++;
    return 0;
}

/* DECODE: split IR into fields and check the opcode is legal. */
static int decode_ir(CPU *cpu)
{
    cpu->dec = decode(cpu->ir);
    if (op_by_code(cpu->dec.op, cpu->dec.rs) == NULL)
        return cpu_error(cpu, "Illegal opcode");
    return 0;
}

/* EXECUTE: carry out the decoded instruction. */
static int execute(CPU *cpu)
{
    Fields f = cpu->dec;
    uint32_t address;

    switch (f.op) {

    case OP_HALT:
        cpu->halted = true;
        break;

    case OP_LDI:
        cpu->regs[f.rd] = (uint16_t)f.imm8;
        break;

    case OP_LOAD:
        address = (f.mode == 0)
                ? (uint32_t)f.addr7
                : (uint32_t)cpu->regs[f.rs] + (uint32_t)f.addr7;

        if (address >= MEM_SIZE)
            return cpu_error(cpu, "LOAD address out of range");

        cpu->regs[f.rd] = read_memory(cpu, (uint16_t)address);
        break;

    case OP_STORE:
        address = (f.mode == 0)
                ? (uint32_t)f.addr7
                : (uint32_t)cpu->regs[f.rs] + (uint32_t)f.addr7;

        if (address >= MEM_SIZE)
            return cpu_error(cpu, "STORE address out of range");

        write_memory(cpu, (uint16_t)address, cpu->regs[f.rd]);
        break;

    case OP_MOV:
        cpu->regs[f.rd] = cpu->regs[f.rs];
        break;

    case OP_ADD:                                /* ALU: Rd + Rs */
        cpu->regs[f.rd] = alu(cpu, '+', cpu->regs[f.rd], cpu->regs[f.rs]);
        break;

    case OP_SUB:                                /* ALU: Rd - Rs */
        cpu->regs[f.rd] = alu(cpu, '-', cpu->regs[f.rd], cpu->regs[f.rs]);
        break;

    case OP_MUL:                                /* ALU: Rd * Rs, low 16 bits */
        cpu->regs[f.rd] = alu(cpu, '*', cpu->regs[f.rd], cpu->regs[f.rs]);
        break;

    case OP_ADDI:                               /* ALU: Rd + signed imm8 */
        cpu->regs[f.rd] = alu(cpu, '+', cpu->regs[f.rd],
                              (uint16_t)sign8(f.imm8));
        break;

    case OP_CMP:                                /* ALU: Rd - Rs, flags only */
        alu(cpu, '-', cpu->regs[f.rd], cpu->regs[f.rs]);
        break;

    case OP_JMP:
        cpu->pc = (uint16_t)f.imm8;
        break;

    case OP_JZ:
        if (cpu->z)
            cpu->pc = (uint16_t)f.imm8;
        break;

    case OP_JNZ:
        if (!cpu->z)
            cpu->pc = (uint16_t)f.imm8;
        break;

    case OP_JN:
        if (cpu->n)
            cpu->pc = (uint16_t)f.imm8;
        break;

    case OP_CALL:
        if (cpu->sp <= STACK_START)
            return cpu_error(cpu, "Stack overflow");

        cpu->sp--;
        write_memory(cpu, cpu->sp, cpu->pc);
        cpu->pc = (uint16_t)f.imm8;
        break;

    case OP_STACK:
        switch (f.rs) {

        case SUB_PUSH:
            if (cpu->sp <= STACK_START)
                return cpu_error(cpu, "Stack overflow");

            cpu->sp--;
            write_memory(cpu, cpu->sp, cpu->regs[f.rd]);
            break;

        case SUB_POP:
            if (cpu->sp >= SP_START)
                return cpu_error(cpu, "Stack underflow");

            cpu->regs[f.rd] = read_memory(cpu, cpu->sp);
            cpu->sp++;
            break;

        case SUB_RET:
            if (cpu->sp >= SP_START)
                return cpu_error(cpu, "Stack underflow");

            cpu->pc = read_memory(cpu, cpu->sp);
            cpu->sp++;
            break;

        default:
            return cpu_error(cpu, "Invalid stack operation");
        }
        break;

    default:
        return cpu_error(cpu, "Unknown opcode");
    }
    return 0;
}

/* Execute exactly one machine instruction: three clock ticks. */
int cpu_step(CPU *cpu)
{
    if (cpu->faulted)
        return -1;
    if (cpu->halted)
        return 0;

    if (fetch(cpu) < 0)
        return -1;
    tick(cpu, PHASE_FETCH);

    if (decode_ir(cpu) < 0)
        return -1;
    tick(cpu, PHASE_DECODE);

    if (execute(cpu) < 0)
        return -1;
    cpu->steps++;
    tick(cpu, PHASE_EXECUTE);
    return 1;
}

/* Run CPU until HALT, error, or execution limit. */
int cpu_run(CPU *cpu, unsigned long max_steps)
{
    while (!cpu->halted && !cpu->faulted &&
           cpu->steps < max_steps) {

        if (cpu_step(cpu) < 0)
            return -1;
    }

    if (cpu->faulted)
        return -1;

    if (!cpu->halted)
        return cpu_error(cpu, "Instruction limit exceeded");

    return 0;
}