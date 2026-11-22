; add.asm -- hand translation of add.c:  result = a + b
        .text
        LOAD  R0, [a]          ; R0 <- a
        LOAD  R1, [b]          ; R1 <- b
        ADD   R0, R1           ; R0 <- R0 + R1   (ALU, sets Z and N)
        STORE R0, [result]     ; result <- R0
        HALT                   ; return 0

        .data
a:      .word 25               ; uint16_t a = 25;
b:      .word 17               ; uint16_t b = 17;
result: .word 0                ; uint16_t result;