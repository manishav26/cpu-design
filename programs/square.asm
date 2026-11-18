; Directly authored assembly: square(8)
.text
    LOAD R0, [input]
    MUL R0, R0
    STORE R0, [result]
    HALT
.data
input: .word 8
result: .word 0
