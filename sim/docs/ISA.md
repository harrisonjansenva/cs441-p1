# ISA Reference

This document mirrors `src/isa.h` / `src/isa.cpp` (the `SIM_OP_LIST` table) and the behavior implemented in `src/cpu.cpp`. If the code and this file disagree, the code is the source of truth.

## Overview

* 32 registers, R0 and R1 are constants (writes to them are ignored)
* 6 bit opcode
* 32 bit word, word addressable
* 26 address bits: `0x3FFFFFF` (`MAX_ADDR`) is the highest addressable data memory location
* 52 instructions defined (64 opcode slots)

## Opcode Map

| Opcode range | Class | Defined |
| :---- | :---- | :---- |
| `000xxx` (0x00-0x07) | Other | 6 |
| `001xxx` (0x08-0x0F) | Memory | 4 |
| `01xxxx` (0x10-0x1F) | ALU | 16 |
| `10xxxx` (0x20-0x2F) | Floating Point | 16 |
| `11xxxx` (0x30-0x3F) | Control | 10 (9 + HALT) |

Any opcode not listed below is illegal; fetching it faults with "illegal instruction". So does fetching from an address with no loaded instruction.

## Registers

* R0 - holds 0, writes ignored
* R1 - holds 1, writes ignored
* R2-R31 - general purpose (all 32 bit; floating point values are stored as raw IEEE-754 single bits in the same registers)
* Flag Register (FR) - `[... n z l e]`, bit 3 = Negative, bit 2 = Zero, bit 1 = Less than, bit 0 = Equal
  * ALU instructions write `n` and `z` from their result and leave `l` and `e` alone
  * `FLT` writes `l`, `FEQ` writes `e`, and each leaves the other bits alone
* Scope Register (SR) - set to `PC + 1` by every `JUMP`, `JUMPR` and `JLINK` (not by branches)
* PC - word index into the program

## Instruction Formats

```
        31    26 25   21 20   16 15   11 10    6 5     0
R-type  | opcode |  rs1  |  rs2  |  rd   |  rs3  |  0    |   register-register ops
I-type  | opcode |  rs1  |  rt   |        imm16          |   LD/ST, ALU-imm, MOV, LDR, branches, JUMPR, JLINK
J-type  | opcode |              target26                 |   JUMP: absolute word address
N-type  | opcode |              unused                   |   NOP, HALT
```

Field positions (from `Operand` / `operandShift`):

| Field | Bits | Notes |
| :---- | :---- | :---- |
| opcode | 31:26 | |
| rs1 | 25:21 | |
| rs2 / rt | 20:16 | Same field. `rt` is used when the register is a destination or a store source |
| rd | 15:11 | |
| rs3 | 10:6 | Only used by FFMA (the "shamt" field) |
| imm16 | 15:0 | Signed (sign extended) |
| target16 | 15:0 | Branch target, unsigned absolute word address |
| target26 | 25:0 | Jump target, unsigned absolute word address |

Unused fields are don't-care to the simulator; the assembler emits 0.

## Other (`000xxx`)

| Opcode | Name | Format | Shape | RTL | Description | Note |
| :---- | :---- | :---- | :---- | :---- | :---- | :---- |
| `0x00` | CLZ | R | RD_RS1 | R[rd] <- clz(R[rs1]) | Count the leading zeros in an integer (32 if the value is 0). | Useful for normalizing or finding the highest set bit. |
| `0x01` | CNTT0 | R | RD_RS1 | R[rd] <- ctz(R[rs1]) | Count the trailing zeros in an integer (32 if the value is 0). | Useful for finding the lowest set bit. |
| `0x02` | POP | R | RD_RS1 | R[rd] <- popcount(R[rs1]) | Count the number of set bits in an integer. | |
| `0x03` | BYTR | R | RD_RS1 | R[rd] <- R[rs1][7:0] ‖ R[rs1][15:8] ‖ R[rs1][23:16] ‖ R[rs1][31:24] | Reverse the byte order of an integer. | Endianness conversion. |
| `0x04` | SEXTD | R | RD_RS1 | R[rd] <- SignExt(R[rs1][15:0]) | Sign extend the low 16 bits of a register to 32 bits. | Turns a 16 bit signed value into a full word. |
| `0x05` | (unused) | | | | | |
| `0x06` | (unused) | | | | | |
| `0x07` | NOP | N | NONE | | Do nothing. | |

None of the Other instructions touch FR.

## Memory (`001xxx`)

`EA = R[rs1] + SignExt(imm16)`. If `EA > 0x3FFFFFF` the simulator faults with "data address out of range".

| Opcode | Name | Format | Shape | RTL | Description | Note |
| :---- | :---- | :---- | :---- | :---- | :---- | :---- |
| `0x08` | LD | I | RT_RS1_IMM | R[rt] <- M[R[rs1] + SignExt(imm16)] | Load a word from memory into a register. | |
| `0x09` | (unused) | | | | | |
| `0x0A` | ST | I | RT_RS1_IMM | M[R[rs1] + SignExt(imm16)] <- R[rt] | Store a register into memory. | |
| `0x0B` | (unused) | | | | | |
| `0x0C` | MOV | I | RT_RS1 | R[rt] <- R[rs1] | Copy one register into another. | imm16 is ignored. Use `MOV rt, R0` to clear a register. |
| `0x0D` | LDR | I | RT_IMM | R[rt] <- SignExt(imm16) | Load a signed 16 bit immediate into a register. | Only reaches -32768..32767; build larger constants with shifts and ADDI. |
| `0x0E` | (unused) | | | | | |
| `0x0F` | (unused) | | | | | |

None of the Memory instructions touch FR.

## ALU (`01xxxx`)

All ALU instructions write `n` (result bit 31) and `z` (result == 0) into FR. Arithmetic wraps modulo 2^32.

| Opcode | Name | Format | Shape | RTL | Description | Note |
| :---- | :---- | :---- | :---- | :---- | :---- | :---- |
| `0x10` | ADD | R | RD_RS1_RS2 | R[rd] <- R[rs1] + R[rs2] | Add two registers. | |
| `0x11` | SUB | R | RD_RS1_RS2 | R[rd] <- R[rs1] - R[rs2] | Subtract rs2 from rs1. | |
| `0x12` | RSB | R | RD_RS1_RS2 | R[rd] <- R[rs2] - R[rs1] | Reverse subtract: subtract rs1 from rs2. | Saves a swap when the operand order is the wrong way around. |
| `0x13` | INC | R | RD_RS1 | R[rd] <- R[rs1] + 1 | Increment a register. | |
| `0x14` | DEC | R | RD_RS1 | R[rd] <- R[rs1] - 1 | Decrement a register. | |
| `0x15` | RS | R | RD_RS1 | R[rd] <- R[rs1] >> 1 | Logical shift right by 1 (zero fill). | Divide by 2 for unsigned values. |
| `0x16` | LS | R | RD_RS1 | R[rd] <- R[rs1] << 1 | Shift left by 1. | Multiply by 2. |
| `0x17` | AND | R | RD_RS1_RS2 | R[rd] <- R[rs1] & R[rs2] | Bitwise AND. | |
| `0x18` | OR | R | RD_RS1_RS2 | R[rd] <- R[rs1] \| R[rs2] | Bitwise OR. | |
| `0x19` | NAND | R | RD_RS1_RS2 | R[rd] <- ~(R[rs1] & R[rs2]) | Bitwise NAND. | |
| `0x1A` | NOR | R | RD_RS1_RS2 | R[rd] <- ~(R[rs1] \| R[rs2]) | Bitwise NOR. | |
| `0x1B` | XOR | R | RD_RS1_RS2 | R[rd] <- R[rs1] ^ R[rs2] | Bitwise XOR. | |
| `0x1C` | NOT | R | RD_RS1 | R[rd] <- ~R[rs1] | Invert all bits. | |
| `0x1D` | ADDI | I | RT_RS1_IMM | R[rt] <- R[rs1] + SignExt(imm16) | Add a signed immediate to a register. | |
| `0x1E` | SUBI | I | RT_RS1_IMM | R[rt] <- R[rs1] - SignExt(imm16) | Subtract a signed immediate from a register. | |
| `0x1F` | SFLIP | R | RD_RS1 | R[rd] <- -R[rs1] | Two's complement negate (flip sign). | |

## Floating Point (`10xxxx`)

Registers hold IEEE-754 single precision values as raw bits. FP instructions do not change `n` or `z`; only `FLT` and `FEQ` write FR.

| Opcode | Name | Format | Shape | RTL | Description | Note |
| :---- | :---- | :---- | :---- | :---- | :---- | :---- |
| `0x20` | FLW | I | RT_RS1_IMM | R[rt] <- M[R[rs1] + SignExt(imm16)] | Load a floating point word from memory. | Same address check as LD. |
| `0x21` | FSW | I | RT_RS1_IMM | M[R[rs1] + SignExt(imm16)] <- R[rt] | Store a floating point word to memory. | Same address check as ST. |
| `0x22` | FADD | R | RD_RS1_RS2 | R[rd] <- R[rs1] + R[rs2] | Add two floats. | |
| `0x23` | FSUB | R | RD_RS1_RS2 | R[rd] <- R[rs1] - R[rs2] | Subtract two floats. | |
| `0x24` | FMULT | R | RD_RS1_RS2 | R[rd] <- R[rs1] * R[rs2] | Multiply two floats. | |
| `0x25` | FDIV | R | RD_RS1_RS2 | R[rd] <- R[rs1] / R[rs2] | Divide two floats. | Division by zero gives inf/NaN, no fault. |
| `0x26` | FFMA | R | RD_RS1_RS2_RS3 | R[rd] <- fma(R[rs1], R[rs2], R[rs3]) | Fused multiply add: (rs1 * rs2) + rs3 with a single rounding. | rs3 is in bits 10:6. Faster and more accurate than FMULT then FADD. |
| `0x27` | FSQRT | R | RD_RS1 | R[rd] <- sqrt(R[rs1]) | Square root of a float. | |
| `0x28` | FNEG | R | RD_RS1 | R[rd] <- R[rs1] ^ 0x80000000 | Flip the sign bit. | Pure bit operation, so it also negates 0 and NaN. |
| `0x29` | FABS | R | RD_RS1 | R[rd] <- R[rs1] & 0x7FFFFFFF | Clear the sign bit. | |
| `0x2A` | FLT | R | RS1_RS2 | FR.l <- (R[rs1] < R[rs2]) ? 1 : 0 | Set the less-than flag if rs1 < rs2 (as floats). | Writes only FR bit 1. No destination register. |
| `0x2B` | FEQ | R | RS1_RS2 | FR.e <- (R[rs1] == R[rs2]) ? 1 : 0 | Set the equal flag if the floats are equal. | Writes only FR bit 0. NaN is never equal. No destination register. |
| `0x2C` | FMAX | R | RD_RS1_RS2 | R[rd] <- (R[rs1] > R[rs2]) ? R[rs1] : R[rs2] | Store the larger of two floats. | If either is NaN, or they are equal, rs2 is returned. |
| `0x2D` | FTOIW | R | RD_RS1 | R[rd] <- (int32)R[rs1] | Convert a float to a signed integer word, truncating toward zero. | NaN gives 0; out of range saturates to 0x7FFFFFFF / 0x80000000. |
| `0x2E` | IWTOF | R | RD_RS1 | R[rd] <- (float)(int32)R[rs1] | Convert a signed integer word to a float. | |
| `0x2F` | FMOV | R | RD_RS1 | R[rd] <- R[rs1] | Copy a float register to another. | Bit copy. |

## Control (`11xxxx`)

Branch and jump targets are absolute word addresses into the program. A target at or beyond the end of the loaded program faults with "jump target outside the program".

| Opcode | Name | Format | Shape | RTL | Description | Note |
| :---- | :---- | :---- | :---- | :---- | :---- | :---- |
| `0x30` | BEQ | I | BR | PC <- (R[rs1] == R[rs2]) ? target16 : PC + 1 | Branch if equal. | |
| `0x31` | BLT | I | BR | PC <- (R[rs1] < R[rs2]) ? target16 : PC + 1 | Branch if less than (signed). | |
| `0x32` | BLE | I | BR | PC <- (R[rs1] <= R[rs2]) ? target16 : PC + 1 | Branch if less than or equal (signed). | |
| `0x33` | BGT | I | BR | PC <- (R[rs1] > R[rs2]) ? target16 : PC + 1 | Branch if greater than (signed). | |
| `0x34` | BGE | I | BR | PC <- (R[rs1] >= R[rs2]) ? target16 : PC + 1 | Branch if greater than or equal (signed). | |
| `0x35` | BNE | I | BR | PC <- (R[rs1] != R[rs2]) ? target16 : PC + 1 | Branch if not equal. | |
| `0x36` | JUMP | J | TGT | SR <- PC + 1; PC <- target26 | Jump to an absolute address. | |
| `0x37` | JUMPR | I | RS1 | SR <- PC + 1; PC <- R[rs1] | Jump to the address held in a register. | Indirect jumps and returns. |
| `0x38` | JLINK | I | RS1_RT | R[rt] <- PC + 1; SR <- PC + 1; PC <- R[rs1] | Jump to the address in rs1 and save the next sequential address in rt. | Subroutine call; return with `JUMPR rt`. |
| `0x3F` | HALT | N | NONE | stop execution | End the program. | |

Notes:

* Branches compare the two registers directly and signed; they do not read FR. Branches do not update SR.
* Branches only carry a 16 bit target (`MAX_BRANCH_TARGET = 0xFFFF`). The simulator accepts programs up to `MAX_PROGRAM_ADDR = 0xFFFFF`; reaching past 0xFFFF needs `JUMP` / `JUMPR`.
* Opcodes `0x39`-`0x3E` are unused.
