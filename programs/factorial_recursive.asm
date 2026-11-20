; Part 2 example: manual translation of factorial_recursive.c.
; main passes n in R1; factorial returns its value in R0.
; R2 is scratch (zero for comparison, saved n after a recursive return).
; Each non-base frame saves n with PUSH; CALL also pushes a return address.
.text
main: LOAD R1, [input]   ; Pass the input value to factorial.
    CALL factorial      ; CPU pushes main's return address, then jumps.
    STORE R0, [result]   ; Save the returned factorial value in data memory.
    HALT                ; All return addresses must be popped; SP = 0x100.

factorial: LDI R2, 0
    CMP R1, R2          ; Base case: if (n == 0) return 1.
    JZ base
    PUSH R1             ; Preserve this call's n before computing n - 1.
    ADDI R1, -1
    CALL factorial      ; Recursive call: R0 receives factorial(n - 1).
    POP R2              ; Restore this frame's n; a deeper call may change R2.
    MUL R0, R2          ; return n * factorial(n - 1).
    RET                 ; Pop the return address and resume the caller.
base: LDI R0, 1         ; 0! = 1.
    RET                 ; Base case has no saved n to pop.
.data
input: .word 4          ; Small default: 4! = 24, with readable call snapshots.
result: .word 0         ; Heap is unused; stack grows from 0xFF downward.
