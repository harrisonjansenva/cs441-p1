# maxfinder-int: find the maximum of an array of signed 32-bit integers.
# Memory (see maxfinder-int.mem):
#   M[0x10] = address where the result is stored
#   M[0x11] = N, number of elements
#   M[0x12] = address of the first element
# Branchless inner loop: MOVGT replaces the compare-and-branch.

    LDR   R10, 0x0011
    LD    R3,  R10, 0        # R3 = N
    LDR   R11, 0x0012
    LD    R2,  R11, 0        # R2 = pointer to Array[0]
    LDR   R12, 0x0010
    LD    R6,  R12, 0        # R6 = output address

    BEQ   R3, R0, DONE       # empty array: nothing to do

    LD    R4, R2, 0          # R4 = max = Array[0]
    DEC   R3, R3
    INC   R2, R2

LOOP:
    BEQ   R3, R0, DONE       # no elements left
    LD    R5, R2, 0          # R5 = Array[i]
    MOVGT R4, R5, R4         # if R5 > R4 then R4 = R5
    INC   R2, R2
    DEC   R3, R3
    JUMP  LOOP

DONE:
    ST    R4, R6, 0          # M[output] = max
    HALT
