# isaSim — CS 441 Project 1 instruction-level simulator

Simulates the team's 32-bit, word-addressable ISA (52 instructions: Other, Memory, ALU,
Floating Point, Control). C++20, standard library + libm only. Tested with g++ 11 (the version on onyx) and Apple clang.

## Build and run

```
$ cd sim
$ make                 # builds build/isaSim_bin  (use `make STD=c++17` on an older compiler)
$ ./isaSim -r <assembly file> <memory file>     # run straight through
$ ./isaSim -s <assembly file> <memory file>     # step by step
```

`./isaSim` is a small launcher script that builds the simulator on first use, so it also works
on a fresh checkout without running `make` first.

Usage: `isaSim -<r|s> <assembly file> <memory file> [-d] [--max-instr N]`

| Flag | Meaning |
|---|---|
| `-r` | Run from PC 0 to `HALT`. |
| `-s` | Step mode. After every step prints PC, the instruction just executed, the next instruction, FR, SR, **all of R0–R31**, and the total instructions executed so far. Prompt: **Enter** = one step, **N** = run N steps, **c** = continue to the end, **q** = quit (outputs are still written). |
| `-d` | Also print the assembled program: address, 32-bit encoding, disassembly. |
| `--max-instr N` | Stop after N instructions (default 2,000,000,000) so a buggy benchmark cannot hang. |

### Outputs (written to the current directory)

At the end of either mode the simulator prints the number of times each instruction executed
(all 52, grouped by class, plus `TOTAL`) and writes:

* `<assembly base name>.instrCount` — the same table (e.g. `maxfinder-fp.asm` → `maxfinder-fp.instrCount`).
* `<memory file name>.out` — final memory (e.g. `maxfinder-fp.mem` → `maxfinder-fp.mem.out`).
  Lists every address named in the input `.mem` plus every address stored to during the run (even if the value is 0),
  sorted, as `0xADDRESS, 0xVALUE`. Any address not listed is 0.

Exit status: 0 on `HALT`; 1 assembly/file error; 3 runtime fault; 4 stopped by `--max-instr` or `q`.

## File formats

**Memory file (`.mem`)**: line 1 a comment naming the benchmark; line 2 `26, 32`
(address bits, bits per word — the simulator requires exactly these); then one `address, value`
per line in hex (`0x` optional; comma and/or spaces; `#` or `;` starts a comment). Unlisted addresses are 0.
Addresses go up to `0x3FFFFFF`. Malformed lines are reported with their line number.

**Assembly file**: one instruction per line; `#` or `;` comments; `label:` definitions (label alone on
a line or before an instruction); a numeric label such as `0x3A:` sets the address of the next instruction
(origin). Registers are `R0`–`R31` (case-insensitive); anything else, such as `R32`, is an error.
Integers are decimal or `0x` hex. Branch/jump targets and `LDR` immediates may be labels, which the
assembler resolves to concrete addresses. Every error is reported as `file:line: error: ...` and nothing runs.

## Instruction syntax

| Operands | Instructions |
|---|---|
| `OP rd, rs1, rs2` | ADD SUB RSB AND OR NAND NOR XOR FADD FSUB FMULT FDIV FMAX |
| `OP rd, rs1` | INC DEC RS LS NOT SFLIP CLZ CNTT0 POP BYTR SEXTD FSQRT FNEG FABS FTOIW IWTOF FMOV |
| `FFMA rd, rs1, rs2, rs3` | rd = rs1·rs2 + rs3 |
| `OP rs1, rs2` | FLT FEQ (set a bit of FR) |
| `OP rt, rs1, imm` | LD FLW (rt = M[rs1+imm]), ST FSW (M[rs1+imm] = rt), ADDI SUBI |
| `MOV rt, rs1` / `LDR rt, imm` | register move / load 16-bit signed immediate (or label address) |
| `Bxx rs1, rs2, target` | BEQ BNE BLT BLE BGT BGE (signed compare, absolute target) |
| `JUMP target` | absolute jump |
| `JUMPR rs1` / `JLINK rs1, rt` | jump to R[rs1]; JLINK also R[rt] ← PC+1 |
| (none) | NOP HALT |

Aliases accepted: `CNTL0` = `CLZ`, `NOOP` = `NOP`.

Encoding (from the spec): R-type `opcode[31:26] rs1[25:21] rs2[20:16] rd[15:11] shamt[10:6] funct[5:0]`;
I-type `opcode rs1 rt imm16`; J-type `opcode target26`. Opcodes are the 6-bit codes in the ISA tables
(`000xxx` Other, `001xxx` Memory, `01xxxx` ALU, `10xxxx` FP, `11xxxx` Control). `funct` is always 0; FFMA's `rs3` is in `shamt`.
`-d` prints the real encodings.

## Behavior the ISA document leaves open (decisions made in the simulator)

| Topic | Decision |
|---|---|
| ALU destination | The ALU table writes `R[rt]` but the format is R-type; we use `rd`. `RSB`: rd = rs2 − rs1. |
| R0 / R1 | Hold 0 / 1 permanently; writes to them are discarded. Only 32 registers exist. |
| Flags FR (`n z l e` = bits 3..0) | ALU-class instructions set n, z from the result; `FLT` sets l; `FEQ` sets e. Branches compare registers directly and do not read FR. FR is visible in step mode. |
| Shifts | `RS` is a logical right shift by 1, `LS` a left shift by 1. |
| Signedness | Branches and `IWTOF` treat registers as signed 32-bit. |
| Branch / jump targets | Absolute instruction addresses (labels are resolved to them). Branch target field is 16 bits (0–65535); `JUMP` 26 bits. |
| Memory model | Separate instruction memory (PC counts instructions from 0) and data memory (word addressed, 26-bit address). |
| SR (scope register) | `SR ← PC+1` on `JUMP`, `JUMPR`, `JLINK`. It is only displayed (step mode); return from a call with `JUMPR` on the link register. |
| Removed instructions | `BEXT`, `IN`, `OUT`, `RET`, `FILL`, `CLR` (tentative in the spec) and `MOVGT`, `MOVLT` are not implemented; their opcodes (`0x05`, `0x06`, `0x39`, `0x3A`–`0x3E`) are illegal. `CLR rd` = `LDR rd, 0`; `FILL rd` = `NOT rd, R0`; return = `JUMPR` on the `JLINK` link register. |
| Bit ops | `CLZ`/`CNTT0` of 0 give 32; `BYTR` reverses the four bytes; `SEXTD` sign-extends bit 15. |
| Floating point | IEEE-754 binary32 held in the ordinary registers (`FLW`/`FSW` are the same word moves as `LD`/`ST`). `FFMA` is a true fused multiply-add. `FMAX` returns rs2 unless rs1 > rs2. `FTOIW` truncates toward zero and saturates (NaN → 0). |
| Faults | Illegal opcode, jumping outside the program, or data address > `0x3FFFFFF` stop the run with a diagnostic; the output files are still written. |

## Design (why it is fast and easy to change)

* `src/isa.h` holds one table (`ISA_LIST`) of all instructions: mnemonic, opcode, format, operand shape, class.
  The assembler, encoder, decoder, disassembler, and the per-instruction counters are all generated from it.
* `src/assembler.cpp` is a two-pass assembler that produces real 32-bit words (range-checked), so the encodings
  in the spec are actually exercised. Words are then decoded **once** into a flat array of `Decoded` records.
* `src/cpu.cpp` has a single `run(cpu, budget)` loop shared by `-r` and `-s`: a dense `switch` over the pre-decoded
  op, PC and flags in locals, one counter increment per instruction, no bounds checks (a sentinel entry after the
  program is an illegal instruction), and writes to R0/R1 redirected at decode time to a scratch slot. Step mode just
  calls it with a budget of 1, so both modes always agree. Throughput is roughly 8×10⁸ instructions/second.
* `src/memory.cpp` is a paged, lazily allocated 2²⁶-word memory (no 256 MB allocation).

## Tests

`make test` (or `sh tests/run_tests.sh`) checks: the three benchmarks against their expected `.mem.out`; `-s` equals `-r`
(memory and counts, including mixed step sizes); every instruction executes in `benchmark1`; MAXFINDER edge cases
(max first/last, all negative, −0.0, single element); assembler and `.mem` errors; R0/R1/CLZ(0) semantics.

## Benchmarks (in `../benchmarks`)

The handout asks for concrete addresses instead of labels, so the `.asm` files here contain none.
`benchmarks/labeled/` holds the readable labeled sources; `tools/resolve_labels.py` turns one into the
other (`python3 tools/resolve_labels.py labeled/x.asm > x.asm`). The simulator also accepts labels directly.

* `maxfinder-int.asm` / `.mem` — maximum of an integer array (compare and branch); result at `M[0x30]` = `0x20`.
* `maxfinder-fp.asm` / `.mem` — same for floats (`FMAX`); result `0x416C0000` = 14.75.
* `benchmark1.asm` / `.mem` — uses all 52 instructions at least once; each result is stored at `0x200 + n`, and `benchmark1.mem.out` lists them.
