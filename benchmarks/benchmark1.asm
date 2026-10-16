# Other
CLZ     R2, R3 # Count leading zeros
CNTT0   R2, R3 # Count trailing zeros
POP     R2, R3 # Population count
BYTR    R2, R3 # Reverse byte order
SEXTD   R2, R3 # Sign extend
MOVGT   R2, R3 # Conditional move if greater
MOVLT   R2, R3 # Conditional move is less than
NOP            # No operation

# Memory
`LDR     R3, 0x1 # Store imm val 0x1 in R4(0x03)
`LD      R4, R0, 0x0100
`LDR     R5, 0x0200
`ST      R5, R0, 0x0100      # M[R[R0] + 0x0100] <- R[R5]
MOV     R6, R4

# ALU
ADD     R7, R4, R6
SUB     R8, R7, R6
RSB     R9, R6, R7      # R9 <- R7 - R6
INC     R8, R8          # R8 <- R8 + 1
DEC     R8, R8          # R8 <- R8 - 1
RS      R9, R9          # R9 <- R9 >> 1
LS      R9, R9      
AND     R10, R4, R6     
OR      R10, R4, R6
NAND    R10, R4, R6
NOR     R10, R4, R6
XOR     R10, R4, R6
NOT     R10, R4
ADDI    R11, R4, 0x0E
SUBI    R11, R11, 0x05
SFLIP   R11, R11

# Floating Point
LDR     R12, 0x0110
FLW     R13, 0x0110
FLW     R14, 0x0111
FADD    R15, R13, R14
FSUB    R15, R13, R14
FMULT   R15, R13, R14
FDIV    R15, R13, R14
FFMA    R16, R13, R14, R15
FSQRT   R16, R13
FNEG    R16, R13
FABS    R16, R13
FLT     R13, R14
FEQ     R13, R14
FMAX    R16, R13, R14
FTOIW   R17, R13
IWTOF   R18, R17
FMOV    R19, R18
FSW     R19, R12

# Control
LDR     R32, 0x01
CLR     R20
FILL    R20
IN      R21
OUT     R21
BEQ     R0, R1, 0x33
BLT     R1, R0, 0x33
BLE     R1, R0, 0x33
BGT     R0, R1, 0x33
BGE     R0, R1, 0x33
BNE     R0, R0, 0x33
LDR     R30, 0x3A
LDR     R31, 0x01
JLINK   R30, R29              
JUMP    0x3C

0x3A:
LS R31, R31
RET

0x3C:
LS R31, R31

LDR     R23, 0x03D0
JUMPR   R23
BEXT    0x003E
HALT