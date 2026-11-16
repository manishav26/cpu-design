
/*
 * cpu.c -- SIMPLE-16 CPU execution engine
 *
 * Implements fetch, decode, execute and status updates.
 * Instruction encoding follows Person A's isa.h / isa.c.
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

/* Read a memory word and update MAR / MDR. */
static uint16_t read_memory(CPU *cpu, uint16_t address)
{
    cpu->mar = address;
    cpu->mdr = cpu->ram[address];
    return cpu->mdr;
}

/* Write a memory word and update MAR / MDR. */
static void write_memory(CPU *cpu, uint16_t address,
                         uint16_t value)
{
    cpu->mar = address;
    cpu->mdr = value;
    cpu->ram[address] = value;
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
}

/* Execute exactly one machine instruction. */
int cpu_step(CPU *cpu)
{
    if (cpu->faulted)
        return -1;

    if (cpu->halted)
        return 0;

    /* FETCH */
    if (cpu->pc > TEXT_END)
        return cpu_error(cpu, "PC outside text segment");

    cpu->ir = read_memory(cpu, cpu->pc);
    cpu->pc++;

    /* DECODE */
    Fields f = decode(cpu->ir);

    if (op_by_code(f.op, f.rs) == NULL)
        return cpu_error(cpu, "Illegal opcode");

    uint16_t result;
    uint32_t address;

    /* EXECUTE */
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

    case OP_ADD:
        result = (uint16_t)(cpu->regs[f.rd] + cpu->regs[f.rs]);
        cpu->regs[f.rd] = result;
        update_flags(cpu, result);
        break;

    case OP_SUB:
        result = (uint16_t)(cpu->regs[f.rd] - cpu->regs[f.rs]);
        cpu->regs[f.rd] = result;
        update_flags(cpu, result);
        break;

    case OP_MUL:
        result = (uint16_t)((uint32_t)cpu->regs[f.rd] *
                            (uint32_t)cpu->regs[f.rs]);
        cpu->regs[f.rd] = result;
        update_flags(cpu, result);
        break;

    case OP_ADDI:
        result = (uint16_t)(cpu->regs[f.rd] + sign8(f.imm8));
        cpu->regs[f.rd] = result;
        update_flags(cpu, result);
        break;

    case OP_CMP:
        result = (uint16_t)(cpu->regs[f.rd] - cpu->regs[f.rs]);
        update_flags(cpu, result);
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

    cpu->steps++;
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
