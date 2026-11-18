# SIMPLE-16 CPU

A 16-bit CPU of our own design, with an assembler, loader, and simulator written in C.
CMPE 220 team project: **CPU Design** (Part 1) and **Program Layout & Execution** (Part 2).

## Build and run (Member 3)

Prerequisites: a C11 compiler (`cc`) and Make. Tests and report generation use
Python 3's standard library. No UI, C compiler implementation, or external C
libraries are needed.

```sh
make
./build/simple16 programs/square.asm
./build/factorial                 # native C reference: prints 40320
./build/simple16 programs/factorial.asm
make run-example
make test
make traces                      # build/traces/{square,factorial}.txt
./build/simple16 --trace programs/square.asm
./build/simple16 --listing programs/factorial.asm
make sanitize                    # UBSan checks; compiler sanitizer support required
make clean
```

`make` compiles the native reference as well as the simulator. To compile the
reference independently: `cc -std=c11 programs/factorial.c -o /tmp/simple16-factorial`
then `/tmp/simple16-factorial`. Its optional input argument is 0..8.
The CPU runs the manually translated assembly; it does not execute native C.
Both examples place input at 0x40 and result at 0x41. Expected results:
square(8) = 64 in 4 instructions; factorial(8) = 40320 in 47 instructions.
Factorial uses R0 for the accumulator, R1 for the countdown and R2 for zero.
Its C function is inlined into the assembly entry program; no compiler is involved.

Normal execution prints a short result summary. Explicit `--trace` mode prints
the loaded state and state after every successful instruction, including HALT.
Each snapshot repeats **all words 0x0000..0x0041**, including the gap between code
and data. The executed address is captured before stepping and is distinct from
the resulting PC. Word addresses and values are fixed-width hex; register values
also show unsigned decimal. N tests bit 15: 40320 is a valid unsigned result even
when N is set. MAR/MDR describe the last memory access, often instruction fetch.
Both modes share `trace_run` and the teammate's `cpu_step`, with a 10,000-instruction
limit and nonzero exit status on faults. The formatter accepts multiple ranges
for later segment views; recursion/heap visualization is not implemented here.

Tests execute actual assembled code, compare full final CPU/RAM state against
`cpu_run`, check read-only formatting, HALT/snapshot counts, full RAM block
coverage, error propagation and step limits, and compare factorial inputs
0, 1, 5 and 8 against native C. Existing CPU issues are recorded separately in
[report/KNOWN_ISSUES.md](report/KNOWN_ISSUES.md).

AddressSanitizer can be requested with `make SANITIZERS=address,undefined sanitize`.
On this machine the combined ASan/UBSan test process stalled both inside and
outside the sandbox and was stopped; no ASan pass is claimed.

### Reproduce the report draft

Install Tectonic for PDF generation, then run:

```sh
make report
```

This runs tests and generates traces, assembler listings, result tables, and
`build/report/report.pdf` from [report/report.tex](report/report.tex) using
[tools/report.py](tools/report.py). LaTeX packages may need an initial download.
Generated artifacts are ignored; all report source is versioned. Python can
generate evidence independently with `python3 tools/report.py` after `make test`.
The title page intentionally requires actual team names. Confirm contributions
and review the outstanding integration issues before submission. This is a
**draft**, not a submission-ready combined report.

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

![SIMPLE-16 CPU diagram](docs/cpu_diagram.png)

### Architecture
| Item | Choice |
|---|---|
| Word size | 16 bits. Every instruction, number, and memory cell is one word |
| Instruction length | Always 1 word (16 bits) |
| Opcode | 4 bits → 16 primary opcode groups, 18 mnemonics. The first hex digit of an instruction is its opcode |
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

![Instruction encoding table](docs/encoding_table.png)

**Hand-encoding example:** `MUL R1, R0`

| opcode | Rd | Rs | unused | → | hex |
|---|---|---|---|---|---|
| `0111` (MUL) | `01` (R1) | `00` (R0) | `00000000` | → | `0x7400` |

More examples: [docs/hand_encoding_examples.png](docs/hand_encoding_examples.png)

### Assembler

Two-pass assembler (`src/assembler.c`):
- **Pass 1:** reads the file, records labels, gives each line an address (`.text` from 0x00, `.data` from 0x40)
- **Pass 2:** parses operands and encodes each line into a 16-bit word

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