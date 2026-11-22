; mul.asm -- hand translation of mul.c:  result = a * b
        .text
        LOAD  R0, [a]          ; R0 <- a
        LOAD  R1, [b]          ; R1 <- b
        MUL   R0, R1           ; R0 <- R0 * R1   (ALU, low 16 bits)
        STORE R0, [result]     ; result <- R0
        HALT                   ; return 0

        .data
a:      .word 6                ; uint16_t a = 6;
b:      .word 7                ; uint16_t b = 7;
result: .word 0                ; uint16_t result;