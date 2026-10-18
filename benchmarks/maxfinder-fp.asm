# FP MAXFINDER


# Load addresses 
LDR R10, 0x0011             # R10 = Address holding array length
LD R3, R10, 0               # R3 = N (loaded dynamically from memory)


LDR R11, 0x0012             # R11 = Address holding array pointer
LD R2, R11, 0               # R2 = Pointer to start of float array


LDR R12, 0x0010             # R12 = Address holding output location
LD R6, R12, 0               # R6 = Output address


# Edge-case check: If N == 0, halt immediately or handle empty array 
BEQ R3, R0, 0x0010


# Load the first element as current max 
FLW R4, R2, 0               # R4 = Array[0]
DEC R3, R3                  # Decrement counter
INC R2, R2                  # Advance pointer to Array[1]


# LOOP: (0x000A)
BEQ R3, R0, 0x0010          # When remaining count is 0, exit
FLW R5, R2, 0               # Load Array[i]
FMAX R4, R4, R5             # R4 = max(R4, R5)
INC R2, R2                  # Move to next word
DEC R3, R3                  # Decrement counter
JUMP 0x000A


# DONE: (0x0010)
FSW R4, R6, 0               # Write result to memory
HALT
