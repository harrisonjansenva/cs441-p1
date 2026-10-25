# CS 441 ISA Simulator
## Team Members:
* Harrison Jansen van Beek
* Eldon Whiteley
* Zach Johnston
* Piper Worsham
* Brad Borgholthaus

Simulates our 32-bit, word-addressable ISA. Tested with g++ 11 (the version on onyx) and Apple clang.

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

### Outputs

At the end of either mode the simulator prints the number of times each instruction executed and writes:

* `<assembly base name>.instrCount` — the same table (e.g. `maxfinder-fp.asm` → `maxfinder-fp.instrCount`).
* `<memory file name>.out` — final memory (e.g. `maxfinder-fp.mem` → `maxfinder-fp.mem.out`).
  Lists every address named in the input `.mem` plus every address stored to during the run (even if the value is 0),
  sorted, as `0xADDRESS, 0xVALUE`. Any address not listed is 0.

Exit status: 0 on `HALT`; 1 assembly/file error; 3 runtime fault; 4 stopped by `--max-instr` or `q`.

## File formats

**Memory file (`.mem`)**: 
  * line 1: a comment naming the benchmark; 
  * line 2 `26, 32` (address bits, bits per word — the simulator requires exactly these);
  * All other lines: one `address, value` per line in hex (`0x` optional; comma and/or spaces; `#` or `;` starts a comment). Unlisted addresses are 0.
Addresses go up to `0x3FFFFFF`. Malformed lines are reported with their line number.

**Assembly file**: 
* one instruction per line; 
* `#` or `;` comments; 
* `label:` definitions
  * label can be alone on a line or before an instruction;
*  a numeric label such as `0x3A:` sets the address of the next instruction (origin). 
*  Registers are `R0`–`R31` (case-insensitive); anything else, such as `R32`, is an error.
*  Integers are decimal or `0x` hex. 
*  Branch/jump targets and `LDR` immediates may be labels, which the assembler resolves to concrete addresses. 
*  Every error is reported as `file:line: error: ...` and nothing runs.

## Instruction syntax

See [this link](docs/ISA.md) for the instruction syntax.
`-d` prints the real encodings.



## Design

* `src/isa.h` holds one table (`ISA_LIST`) of all instructions: mnemonic, opcode, format, operand shape, class.
  The assembler, encoder, decoder, disassembler, and the per-instruction counters are all generated from it.
* `src/assembler.cpp` is a two-pass assembler that produces real 32-bit words (range-checked), so the encodings
  in the spec are actually exercised. Words are then decoded **once** into a flat array of `Decoded` records.
* `src/cpu.cpp` has a single `run(cpu, budget)` loop shared by `-r` and `-s`: a dense `switch` over the pre-decoded
  op, PC and flags in locals, one counter increment per instruction, no bounds checks (a sentinel entry after the
  program is an illegal instruction), and writes to R0/R1 redirected at decode time to a scratch slot. Step mode just
  calls it with a budget of 1, so both modes always agree. Throughput is roughly 8×10⁸ instructions/second.
* `src/memory.cpp` is a paged, lazily allocated 2²⁶-word memory. We did end up writing an MMU! :)

## Tests

`make test` (or `sh tests/run_tests.sh`) checks: 
* the three benchmarks against their expected `.mem.out`; 
* `-s` equals `-r`
* every instruction executes in `benchmark1`; 
* MAXFINDER edge cases (max first/last, all negative, −0.0, single element); 
* assembler and `.mem` errors; 
* R0/R1/CLZ(0) semantics.

## AI Use

AI was used to assist in this project's development. However, large design decisions were still made by the team and all code written was audited for correctness and logical implementation.
