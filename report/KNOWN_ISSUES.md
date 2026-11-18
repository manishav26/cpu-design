# Integration review (existing teammate code)

Verified against local `origin/cpu-simulator` ae9e0a6. No fetch was needed;
remote GitHub merge status was not queried. ISA/assembler files are identical
to local `origin/cpudesign/isa-assembler`. No CPU/assembler changes are included.

| Owner | Finding | Evidence / impact |
|---|---|---|
| Member 2 | MUL integer promotions | `cpu.c` multiplies two uint16_t values after promotion to signed int. 65535 squared exceeds INT_MAX; widen operands to uint32_t before multiplication. |
| Member 2 | Indirect address narrowing | LOAD/STORE cast base+offset to uint16_t before checking MEM_SIZE. 65535+65 wraps to 64. Calculate and check in uint32_t first. |
| Member 1 | Branch target domain | Assembler accepts 0..255; CPU permits execution only at 0..63. A jump to 64 assembles, then faults on the next fetch. |
| Member 1 | Data literal truncation | `.word 65536` becomes 0 via masking. Reject or document explicitly. |
| Member 1 | Missing final newlines | isa.h, isa.c, assembler.h, assembler.c produce Clang -Wnewline-eof warnings under -Wpedantic. |

README diagram paths and the misleading “16 instructions” description were
corrected as documentation integration. There are 16 primary groups and 18 mnemonics.

The contributed examples never use overflowing host-int products or invalid
indirect addresses. Passing example sanitizers does not resolve these issues.

Manual probes (keep outside the normal passing test suite):

```asm
; MUL overflow: UBSan should report signed integer overflow.
.text
LOAD R0, [large]
MUL R0, R0
HALT
.data
large: .word 65535
```

```asm
; Address wrap: CPU incorrectly reads data at 0x40 instead of faulting.
.text
LOAD R0, [large]
LOAD R1, [R0+65]
HALT
.data
large: .word 65535
```

Do not label the combined submission final until names, teammate review and
integration decisions about these issues are complete. The report contains
complete traces for the required examples, not the later recursion assignment.
