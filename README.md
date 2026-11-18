# SIMPLE-16 CPU

A 16-bit CPU of our own design, with an assembler, loader, and simulator written in C.
CMPE 220 team project: **CPU Design** (Part 1) and **Program Layout & Execution** (Part 2).

## Part 1: CPU Design

### Architecture Overview

```
┌──────────────────────────────────────────────────────────────────┐
│           SIMPLE-16 CPU  (16-bit words, 4-bit opcode)            │
│                                                                  │
│   ┌──────────────┐    ┌──────────────┐    ┌────────────────────┐ │
│   │ Control Unit │    │     ALU      │    │     Registers      │ │
│   │              │    │              │    │                    │ │
│   │    Fetch     │    │   ADD  SUB   │    │   R0  R1  R2  R3   │ │
│   │    Decode    │    │   MUL  CMP   │    │     PC  IR  SP     │ │
│   │   Execute    │    │     ADDI     │    │      MAR  MDR      │ │
│   │              │    │  flags Z, N  │    │       FLAGS        │ │
│   └──────────────┘    └──────────────┘    └────────────────────┘ │
│           │                   │                      │           │
│           └───────────────────┼──────────────────────┘           │
│                               │  16-bit internal bus             │
│                               │  MAR = address, MDR = data       │
│                               ▼                                  │
│           ┌──────────────────────────────────────┐               │
│           │ MEMORY  (256 words x 16 bits)        │               │
│           │                                      │               │
│           │ 0x00 - 0x3F   Text    code           │               │
│           │ 0x40 - 0x7F   Data    variables      │               │
│           │ 0x80 - 0xBF   Heap    dynamic data   │               │
│           │ 0xC0 - 0xFF   Stack   grows down     │               │
│           └──────────────────────────────────────┘               │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

![SIMPLE-16 CPU diagram](docs/1_cpu_diagram.png)

### Architecture
| Item | Choice |
|---|---|
| Word size | 16 bits. Every instruction, number, and memory cell is one word |
| Instruction length | Always 1 word (16 bits) |
| Opcode | 4 bits → 16 instructions. The first hex digit of an instruction is its opcode |
| General registers | R0, R1, R2, R3 (16-bit) |
| Special registers | PC (program counter), IR (instruction register), FLAGS, SP (stack pointer), MAR (memory address register), MDR (memory data register) |
| Flags | Z = result is zero, N = result is negative (bit 15). Set by ADD, SUB, MUL, ADDI, CMP |
| Memory | 256 words × 16 bits, addresses 0x00–0xFF |

### Memory Layout
| Segment | Addresses | Holds |
|---|---|---|
| Text | 0x00–0x3F | Program code |
| Data | 0x40–0x7F | Global variables |
| Heap | 0x80–0xBF | Dynamic data (used in Part 2) |
| Stack | 0xC0–0xFF | Return addresses and saved registers (used in Part 2). Grows down from 0xFF |

### Instruction Formats
Bit 15 is on the left.
```
R  [ op 4 | Rd 2 | Rs 2  | 0000 0000     ]   MOV ADD SUB MUL CMP
I  [ op 4 | Rd 2 | 00    | imm 8         ]   LDI ADDI
M  [ op 4 | Rd 2 | Rs 2  | M 1 | addr 7  ]   LOAD STORE
J  [ op 4 | 00   | 00    | addr 8        ]   JMP JZ JNZ JN CALL
S  [ op 4 | Rd 2 | sub 2 | 0000 0000     ]   PUSH POP RET
N  [ op 4 | 0000 0000 0000               ]   HALT
```
- **Register codes:** R0 = 00, R1 = 01, R2 = 10, R3 = 11
- **M bit (LOAD/STORE):**
  - 0 = direct, address = addr7 (0x00–0x7F)
  - 1 = register + offset, address = Rs + offset (0–127)
- **sub field (opcode F):** 00 = PUSH, 01 = POP, 10 = RET

### Instruction Encoding

![Instruction encoding table](docs/8_encoding_table.png)

**Hand-encoding example:** `MUL R1, R0`

| opcode | Rd | Rs | unused | → | hex |
|---|---|---|---|---|---|
| `0111` (MUL) | `01` (R1) | `00` (R0) | `00000000` | → | `0x7400` |

More examples: [docs/5_hand_encoding_examples.png](docs/5_hand_encoding_examples.png)

### Assembler

Two-pass assembler (`src/assembler.c`):
- **Pass 1:** reads the file, records labels, gives each line an address (`.text` from 0x00, `.data` from 0x40)
- **Pass 2:** parses operands and encodes each line into a 16-bit word
- Reports an error for jump targets outside the text segment and `.word` values that don't fit in 16 bits

```asm
        .text
start:  LOAD  R0, [base]     ; direct address
        LDI   R2, 1
loop:   MUL   R2, R0
        JNZ   loop
        HALT
        .data
base:   .word 8
```

## Part 2: Program Layout & Execution
_To be added._