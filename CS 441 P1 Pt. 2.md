- Instruction name  
  - Instruction format (e.g. opcode | register | register. Something comparable to the R-type, I-type, J-type instruction formats given in MIPS)  
  - RTL description of instruction (e.g. PC ← PC \+ 1, M(X) ← R(Y), etc.)  
  - Description of instruction in words  
  - For each instruction that is not obvious, include a note for when it is useful (e.g. ADD does not need an additional note. ShiftLeft might warrant a note. FP-Arith would definitely warrant a note.) Use your best judgement on which instructions need a note. (Tip: If any of the members of the group are unsure, it should have a note) 

32 registers \- 8 non-addressable  
6 bit opcode  
26 address bits  
Byte addressable  
32 bit word  
64 Instructions

ALU: Piper  
MEM: Zach  
Control:Brad  
FP: Eldon Whiteley  
Other: Harrison

`000xxx - ALU - 8 instructions`  
`001xxx - Memory - 8 instructions`  
`01xxxx - Other - 16 Instructions`  
`10xxxx - FP - 16 Instructions`  
`11xxxx - Control - 15 Instructions`  
`111111 - Halt`

# ALU:

| Name | Format | RTL Description  | Description | Note |
| :---- | :---- | :---- | :---- | :---- |
| 000000 \- ADD |  | R(D) \<- R(S1) \+ R(S2) | Adds S1 to S2 |  |
| 000001 \- SUB |  | R(D) \<- R(S1) \- R(S2) | Subtracts S2 from S1 |  |
| 000010 \- RS |  | R(D) \<- R(S1) \>\> R(S2) | Shifts the bits of S1 to the right 1 |  |
| 000011 \- LS |  | R(D) \<- R(S1) \<\< 1 | Shifts the bits of S1 to the left 1 |  |
| 000100 \- AND |  | R(D) \<- R(S1)  && R(S2) | AND of S1 and S2 |  |
| 000101 \- OR |  | R(D) \<- R(S1) || R(S2) | OR of S1 and S2 |  |
| 000110 \- XOR |  | R(D) \<- R(S1)  $\oplus$ R(S2) | Exclusive OR of S1 and S2 |  |
| 000111 \- NOT |  | R(D) \<- \~ R(S1)  | Inverts all bits of S1 |  |

# Memory:

| Name | Format | RTL Description  | Description | Note |
| :---- | :---- | :---- | :---- | :---- |
|  |  |  |  |  |
|  |  |  |  |  |
|  |  |  |  |  |
|  |  |  |  |  |
|  |  |  |  |  |
|  |  |  |  |  |
|  |  |  |  |  |
|  |  |  |  |  |

# Other:

| Name | Format | RTL Description  | Description | Note |
| :---- | :---- | :---- | :---- | :---- |
| CNTL0 | 01 \- 0000 |  |  |  |
| CNTT0 | 01 \- 0001 |  |  |  |
| POP | 01 \- 0010 |  |  |  |
| BITR | 01 \- 0011 |  |  |  |
| BYTR | 01 \- 0100 |  |  |  |
| CONDMV | 01 \- 0101 |  |  |  |
| SADD | 01 \- 0110 |  |  |  |
| SSUB | 01  \- 0111 |  |  |  |
| SEXTD | 01 \- 1000 |  |  |  |
| NOOP | 01 \- 1001 |  |  |  |
|  | 01 \- 1010 |  |  |  |
|  | 01 \- 1011 |  |  |  |
|  | 01 \- 1100 |  |  |  |
|  | 01 \- 1101 |  |  |  |
|  | 01 \- 1110 |  |  |  |
| HALT | 01 \- 1111 |  |  |  |

# Floating Point:

| Code | Name | Format | RTL Description  | Description | Note |
| :---- | :---- | :---- | :---- | :---- | :---- |
| `10 - 0000` |  |  |  |  |  |
| `10 - 0001` |  |  |  |  |  |
| `10 - 0010` |  |  |  |  |  |
| `10 - 0011` |  |  |  |  |  |
| `10 - 0100` |  |  |  |  |  |
| `10 - 0101` |  |  |  |  |  |
| `10 - 0110` |  |  |  |  |  |
| `10 - 0111` |  |  |  |  |  |
| `10 - 1000` |  |  |  |  |  |
| `10 - 1001` |  |  |  |  |  |
| `10 - 1010` |  |  |  |  |  |
| `10 - 1011` |  |  |  |  |  |
| `10 - 1100` |  |  |  |  |  |
| `10 - 1101` |  |  |  |  |  |
| `10 - 1110` |  |  |  |  |  |
| `10 - 1111` |  |  |  |  |  |

# Control:

| Name | Format | RTL Description  | Description | Note |
| :---- | :---- | :---- | :---- | :---- |
| 11 \- 0000 |  |  |  |  |
| 11 \- 0001 |  |  |  |  |
| 11 \- 0010 |  |  |  |  |
| 11 \- 0011 |  |  |  |  |
| 11 \- 0100 |  |  |  |  |
| 11 \- 0101 |  |  |  |  |
| 11 \- 0110 |  |  |  |  |
| 11 \- 0111 |  |  |  |  |
| 11 \- 1000 |  |  |  |  |
| 11 \- 1001 |  |  |  |  |
| 11 \- 1010 |  |  |  |  |
| 11 \- 1011 |  |  |  |  |
| 11 \- 1100 |  |  |  |  |
| 11 \- 1101 |  |  |  |  |
| 11 \- 1110 |  |  |  |  |
| 11 \- 1111 |  | HALT |  |  |

