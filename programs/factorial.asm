; Part 1: manual translation of the iterative logic in factorial.c.
; R0 = accumulator, R1 = remaining counter n, R2 = constant zero.
; The C function is inlined here; this example does not create stack frames.
.text
    LOAD R1, [input]     ; n = input (supported demonstration inputs: 0..8).
    LDI R0, 1           ; acc = 1, also giving 0! and 1! their correct result.
    LDI R2, 0           ; CMP needs a register containing zero.
loop: CMP R1, R2        ; Test n, not the product: 8! has bit 15 set.
    JZ done             ; while (n != 0): exit before multiplying by zero.
    MUL R0, R1          ; acc = acc * n; result is an unsigned 16-bit word.
    ADDI R1, -1         ; n--.
    JMP loop            ; Recheck the condition for the next iteration.
done: STORE R0, [result] ; Return the final accumulator through labeled RAM.
    HALT                ; A successful run ends here, before the step limit.
.data
input: .word 8          ; Input and output stay separate for trace comparison.
result: .word 0         ; 8! = 40320 = 0x9D80 (unsigned).
