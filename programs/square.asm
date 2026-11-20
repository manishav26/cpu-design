; Part 1: directly expressed square(8), with no C translation step.
; R0 holds the input first and the product afterward. Addresses are word addresses.
.text
    LOAD R0, [input]     ; Read the input from the labeled data cell.
    MUL R0, R0          ; R0 = R0 * R0 = 64; arithmetic updates Z and N.
    STORE R0, [result]   ; Make the answer visible in RAM as well as R0.
    HALT                ; Stop normally; the trace includes this instruction.
.data
input: .word 8          ; Test input at 0x40.
result: .word 0         ; Output at 0x41, initially zero.
