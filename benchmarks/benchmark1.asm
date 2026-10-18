# benchmark1: exercises every instruction in the ISA at least once.
# Integer inputs a=R6=5, b=R7=9. Results are stored at 0x200.. (one per word).

# ---- Memory: LDR, LD, MOV, ST ----
LDR R2, 0x100               # data base
LD R3, R2, 0                # 0x00F00000
LD R4, R2, 1                # 0x11223344
LD R5, R2, 2                # 0x00008001
LD R6, R2, 3                # a = 5
LD R7, R2, 4                # b = 9
LDR R20, 0x200              # output base
MOV R9, R6                  # R9 = 5
ST R9, R20, 0               # M[0x200] = 0x00000005

# ---- Other: CLZ CNTT0 POP BYTR SEXTD MOVGT MOVLT NOP ----
CLZ R8, R3
ST R8, R20, 1               # M[0x201] = 0x00000008
CNTT0 R8, R3
ST R8, R20, 2               # M[0x202] = 0x00000014
POP R8, R3
ST R8, R20, 3               # M[0x203] = 0x00000004
BYTR R8, R4
ST R8, R20, 4               # M[0x204] = 0x44332211
SEXTD R8, R5
ST R8, R20, 5               # M[0x205] = 0xFFFF8001
LDR R8, 0x77
MOVGT R8, R7, R6            # 9 > 5: R8 = 9
ST R8, R20, 6               # M[0x206] = 0x00000009
LDR R8, 0x77
MOVLT R8, R6, R7            # 5 < 9: R8 = 5
ST R8, R20, 7               # M[0x207] = 0x00000005
LDR R9, 0x55
MOVGT R9, R6, R7            # 5 > 9 false: R9 unchanged
ST R9, R20, 8               # M[0x208] = 0x00000055
NOP

# ---- ALU ----
ADD R8, R6, R7
ST R8, R20, 9               # M[0x209] = 0x0000000E
SUB R8, R7, R6
ST R8, R20, 10              # M[0x20A] = 0x00000004
RSB R8, R7, R6              # rs2 - rs1 = 5 - 9
ST R8, R20, 11              # M[0x20B] = 0xFFFFFFFC
INC R8, R6
ST R8, R20, 12              # M[0x20C] = 0x00000006
DEC R8, R6
ST R8, R20, 13              # M[0x20D] = 0x00000004
RS R8, R7                   # 9 >> 1
ST R8, R20, 14              # M[0x20E] = 0x00000004
LS R8, R7                   # 9 << 1
ST R8, R20, 15              # M[0x20F] = 0x00000012
AND R8, R6, R7
ST R8, R20, 16              # M[0x210] = 0x00000001
OR R8, R6, R7
ST R8, R20, 17              # M[0x211] = 0x0000000D
NAND R8, R6, R7
ST R8, R20, 18              # M[0x212] = 0xFFFFFFFE
NOR R8, R6, R7
ST R8, R20, 19              # M[0x213] = 0xFFFFFFF2
XOR R8, R6, R7
ST R8, R20, 20              # M[0x214] = 0x0000000C
NOT R8, R6
ST R8, R20, 21              # M[0x215] = 0xFFFFFFFA
ADDI R8, R6, 100
ST R8, R20, 22              # M[0x216] = 0x00000069
SUBI R8, R6, 7
ST R8, R20, 23              # M[0x217] = 0xFFFFFFFE
SFLIP R8, R7
ST R8, R20, 24              # M[0x218] = 0xFFFFFFF7

# ---- Floating point (3.0 = R11, 2.0 = R12, 4.0 = R13) ----
LDR R10, 0x110
FLW R11, R10, 0
FLW R12, R10, 1
FLW R13, R10, 2
FADD R14, R11, R12
FSW R14, R20, 25            # M[0x219] = 0x40A00000
FSUB R14, R11, R12
FSW R14, R20, 26            # M[0x21A] = 0x3F800000
FMULT R14, R11, R12
FSW R14, R20, 27            # M[0x21B] = 0x40C00000
FDIV R14, R11, R12
FSW R14, R20, 28            # M[0x21C] = 0x3FC00000
FFMA R14, R11, R12, R13     # 3*2 + 4
FSW R14, R20, 29            # M[0x21D] = 0x41200000
FSQRT R14, R13
FSW R14, R20, 30            # M[0x21E] = 0x40000000
FNEG R14, R11
FSW R14, R20, 31            # M[0x21F] = 0xC0400000
FABS R14, R14
FSW R14, R20, 32            # M[0x220] = 0x40400000
FLT R12, R11                # FR.l = (2.0 < 3.0) = 1 (visible in step mode)
FEQ R11, R11                # FR.e = 1 (visible in step mode)
FMAX R14, R11, R12
FSW R14, R20, 33            # M[0x221] = 0x40400000
FTOIW R14, R13              # 4.0 -> 4
ST R14, R20, 34             # M[0x222] = 0x00000004
IWTOF R14, R6               # 5 -> 5.0
FSW R14, R20, 35            # M[0x223] = 0x40A00000
FMOV R15, R11
FSW R15, R20, 36            # M[0x224] = 0x40400000

# ---- Control: branches taken (each skips a poison LDR) ----
CLR R16
BEQ R6, R6, 0x005E          # taken
LDR R16, 0xBAD              # poison
# TK0: (0x005E)
BLT R6, R7, 0x0060          # taken
LDR R16, 0xBAD              # poison
# TK1: (0x0060)
BLE R6, R6, 0x0062          # taken
LDR R16, 0xBAD              # poison
# TK2: (0x0062)
BGT R7, R6, 0x0064          # taken
LDR R16, 0xBAD              # poison
# TK3: (0x0064)
BGE R7, R7, 0x0066          # taken
LDR R16, 0xBAD              # poison
# TK4: (0x0066)
BNE R6, R7, 0x0068          # taken
LDR R16, 0xBAD              # poison
# TK5: (0x0068)
ST R16, R20, 37             # M[0x225] = 0x00000000

# ---- Control: branches not taken (each adds 1 to R16) ----
BEQ R6, R7, 0x006B          # not taken
INC R16, R16
# NT0: (0x006B)
BLT R7, R6, 0x006D          # not taken
INC R16, R16
# NT1: (0x006D)
BLE R7, R6, 0x006F          # not taken
INC R16, R16
# NT2: (0x006F)
BGT R6, R7, 0x0071          # not taken
INC R16, R16
# NT3: (0x0071)
BGE R6, R7, 0x0073          # not taken
INC R16, R16
# NT4: (0x0073)
BNE R6, R6, 0x0075          # not taken
INC R16, R16
# NT5: (0x0075)
ST R16, R20, 38             # M[0x226] = 0x00000006

# ---- Control: JUMP, JUMPR, JLINK/RET, BEXT ----
JUMP 0x0078
LDR R16, 0xBAD              # poison
# J1: (0x0078)
LDR R17, 0x007B
JUMPR R17
LDR R16, 0xBAD              # poison
# J2: (0x007B)
CLR R19
LDR R17, 0x008D
JLINK R17, R18              # call; R18 = return address
INC R19, R19                # runs after RET
ST R19, R20, 39             # M[0x227] = 0x00000003
BEQ R0, R0, 0x0082          # taken branch lands on BEXT
LDR R16, 0xBAD              # poison
# BX: (0x0082)
BEXT 0x0084                 # arrived via taken branch: jumps
LDR R16, 0xBAD              # poison
# BT: (0x0084)
ST R16, R20, 40             # M[0x228] = 0x00000006

# ---- Control: IN, OUT, FILL, CLR ----
IN R22                      # stdin integer; reads 0 at EOF
OUT R22
ST R22, R20, 41             # M[0x229] = 0x00000000
FILL R21
ST R21, R20, 42             # M[0x22A] = 0xFFFFFFFF
CLR R21
ST R21, R20, 43             # M[0x22B] = 0x00000000
HALT

# ---- subroutine ----
# SUBR: (0x008D)
INC R19, R19
INC R19, R19
RET
