# maxfinder-int: find the maximum of an array of signed 32-bit integers.
# Memory (see maxfinder-int.mem):
#   M[0x10] = address where the result is stored
#   M[0x11] = N, number of elements
#   M[0x12] = address of the first element

LDR R10, 0x0011
LD R3,  R10, 0              # R3 = N
LDR R11, 0x0012
LD R2,  R11, 0              # R2 = pointer to Array[0]
LDR R12, 0x0010
LD R6,  R12, 0              # R6 = output address

BEQ R3, R0, 0x0011          # empty array: nothing to do

LD R4, R2, 0                # R4 = max = Array[0]
DEC R3, R3
INC R2, R2

# LOOP: (0x000A)
BEQ R3, R0, 0x0011          # no elements left
LD R5, R2, 0                # R5 = Array[i]
BLE R5, R4, 0x000E          # not larger: keep the current max
MOV R4, R5                  # new max
# SKIP: (0x000E)
INC R2, R2
DEC R3, R3
JUMP 0x000A

# DONE: (0x0011)
ST R4, R6, 0                # M[output] = max
HALT
