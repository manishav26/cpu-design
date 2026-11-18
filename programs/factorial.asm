; Manual translation of factorial.c; R0=acc, R1=n, R2=zero
.text
    LOAD R1, [input]
    LDI R0, 1
    LDI R2, 0
loop: CMP R1, R2
    JZ done
    MUL R0, R1
    ADDI R1, -1
    JMP loop
done: STORE R0, [result]
    HALT
.data
input: .word 8
result: .word 0
