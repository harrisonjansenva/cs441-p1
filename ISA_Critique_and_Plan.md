# CS 441 Project 1 — ISA Critique & Project Plan

## Context

The team is 4 days from the **Oct 5 ISA Specification** deadline (150 pts), with Benchmarks due Oct 12 (200 pts) and the Simulator due Oct 19 (250 pts). The current working doc (`CS 441 P1 Pt. 2.md`) has global parameters chosen, opcode space partitioned by team member, and only the 8 ALU instructions filled in — every `Format` column is empty, and Memory / FP / Control tables are entirely blank.

The problem this analysis addresses: the opcode map was allocated by **headcount quota** (8/8/16/16/15 slots per person) rather than by what each instruction class actually needs. That inversion has already produced a duplicate HALT, starved the ALU of immediate forms, and reserved a quarter of the encoding space for bit-manipulation exotica. Separately, the chosen global parameters are internally inconsistent in one load-bearing way (byte addressing vs. the 6+26=32 split). Both are cheap to fix now and expensive to fix after the spec is submitted and the simulator is half-written.

Intended outcome: a frozen, coherent encoding by **Oct 3**, leaving two days to fill RTL/description/notes tables, and a simulator skeleton running during benchmark week so benchmarks are validated by execution rather than by hand-tracing.

---

# PART 1 — CRITIQUE

*(This section is written to be lifted out and circulated to the team as-is.)*

## 1. The binding constraint is the coverage benchmark, not opcode space

You have 64 opcode slots and have planned to fill all 64. But the deliverables require:

> "A benchmark that uses **every** instruction in your ISA, **the expected memory contents after the benchmark executes**" — 50 pts

Every instruction you define is a line of assembly you must write *and* a result you must be able to justify. 64 instructions means 64 hand-verified outcomes — including `CNTL0`, `CNTT0`, `POP`, `BITR`, `BYTR`, and saturating add. You also owe RTL (40 pts) and a format diagram (40 pts) for each one.

**Instruction count is a liability in this project, not a feature.** Target **~40 instructions**, with anything beyond that added only if someone volunteers to write the benchmark lines and expected results for it (§2.5). This single reframing justifies most of what follows.

## 2. Addressing granularity — the one decision to settle before anything else

Current parameters: `6 bit opcode`, `26 address bits`, `byte addressable`, `32 bit word`. The reasoning behind byte addressability was "match modern, fast ISAs like MIPS and ARM." That reasoning is correct about real hardware, so it deserves a real answer rather than a dismissal.

### Why byte addressability won in real ISAs

1. **Character and string data.** The decisive historical reason. IBM's System/360 (1964) chose byte addressing for commercial record processing, and C's memory model baked it in — `sizeof(char) == 1`, `char*` arithmetic, and `memcpy` all assume the addressable unit is a byte.
2. **Mixed-width structs.** Packing `char`/`short`/`int` into one record requires sub-word addressing.
3. **I/O, network packets, and file formats**, which are byte streams and often contain unaligned fields.

Word-addressable machines were not the losers of a performance argument — they were optimized for a different workload. The CDC 6600, PDP-10, and **Cray-1** were all word-addressable, and the Cray-1 was the fastest computer in the world. Many DSPs are still word-addressable today. The real equation is not *modern = byte-addressable*; it is **general-purpose-with-text = byte-addressable, numeric = word-addressable**.

Both of your required benchmarks are MAXFINDER over an integer array and a float array. This machine's entire workload is numeric. There is no character processing anywhere in the project.

### The specific thing that flagged this: your 6+26 split is already ARM's

You independently wrote down `6 bit opcode` + `26 address bits`. That is not an arbitrary coincidence — it is almost exactly these two encodings:

```
MIPS   j      | 000010 | target26 |    PC ← (PC+4)[31:28] : target26 : 00
ARM64  B      | 000101 | imm26    |    PC ← PC + (imm26 << 2)
```

Both are a 6-bit opcode plus a 26-bit field. **In both, that 26-bit field counts instructions, not bytes.** MIPS and ARM are byte-addressable for *data* but word-scale every instruction-address field, because a field that counted bytes would waste its low 2 bits and reach only a quarter as far. MIPS's `j` can only reach a 256 MiB region — a direct scar from this — which is why MIPS also needs `jr` for far jumps.

So the instinct that produced 6+26 was an instruction-granularity instinct. The question is only whether to extend that granularity to the data space too.

### The three real options

| | **A — Word-addressable** | **B — Full MIPS/ARM fidelity** | **C — Byte-addressable, word-only access** |
|---|---|---|---|
| Data granularity | 32-bit word | byte | byte (but unreachable) |
| `.mem` header | `26,32` | `26,8` | `26,8` |
| Sequential PC | `PC ← PC + 1` | `PC ← PC + 4` | `PC ← PC + 4` |
| Branch RTL | `PC ← PC+1+sext(off)` | `PC ← PC+4+(sext(off)<<2)` | same as B |
| Extra instructions | — | `LB` `LBU` `LH` `LHU` `SB` `SH` | none |
| Endianness | n/a | must specify | must specify, unobservable |
| Alignment rules | n/a | must specify | must specify |
| 100-word array in `.mem` | 100 lines | 400 lines | 400 lines |
| Realism | numeric-machine precedent | highest | lowest |

**Option C is the trap, and it is the default you drift into if you don't decide.** It pays every cost of byte addressing — the `<<2` in every branch, alignment, endianness, 4× memory files — and collects none of the benefit, because without `LB`/`SB` the machine can only touch whole words anyway; the addresses just count by 4 instead of by 1. There is real precedent for exactly this mistake: the original DEC Alpha 21064 was byte-addressable but shipped with **no byte load or store**, forcing extract/mask sequences. It was widely considered a design error and byte access was retrofitted in the BWX extension.

### Concrete costs of Option B in *this* project

Byte addressing is only worth paying for if you use it, and using it means Option B. What that costs here:

- **Six more instructions** (`LB`, `LBU`, `LH`, `LHU`, `SB`, `SH`), each needing a format diagram, RTL, description, note, simulator handler, **a line in the coverage benchmark, and a hand-verified expected memory value**. Plus a sign-extension rule each (hence the `U` variants).
- **The hardest hand-verification in the project.** Checking the expected output of a byte store means reasoning about which byte lane inside which word changed, in the endianness you picked. This is the single most error-prone thing in the 50-point coverage benchmark.
- **Hand-authoring the `.mem` files becomes miserable.** Placing the value `42` at one array slot:

  ```
  Option A (26,32):        Option B (26,8), little-endian:
  000100,0000002A          000400,2A
                           000401,00
                           000402,00
                           000403,00
  ```
  A 100-element array is 400 lines, byte-reversed, and the expected-output file is another 400. There is no mixed path here either — declaring `26,32` while the ISA addresses bytes means the memory file and the ISA use different address units, which a grader reading for "Coherence of ISA" will catch.
- **`<<2` and `+4` in ~9 branch/jump RTL lines**, each of which must match the simulator and every hand-traced branch in the benchmarks. Off-by-four in a branch target is the classic version of this bug.
- **Every loop gains a stride conversion.** Word: `ADDI rp, rp, 1`. Byte: `ADDI rp, rp, 4`, and index→address needs `SLL rt, ri, 2`. Minor individually, but it touches every loop you write, in all three benchmarks.

### Recommendation: Option A

**Word-addressable, 26-bit word address, 32 bits per address** (256 MiB of data), `PC ← PC + 1`, branch offsets in instructions, J-target an absolute word address using all 26 bits with nothing wasted.

What you give up, stated honestly: no sub-word access, so byte or character work would need shift-and-mask. You are not doing byte or character work.

What you gain is not just simplicity — it is that **every parameter in the design becomes mutually consistent**. The 26-bit field spans exactly the address space. The handout's own RTL examples (`PC ← PC + 1`, `M(X) ← R(Y)`) are satisfied literally. No rule in the spec exists only to paper over a unit mismatch.

### How to get the realism credit anyway

Do not hide the choice — spend three sentences on it in the spec:

> This machine is word-addressable with 32-bit words. Byte addressability in MIPS and ARM exists to serve character and record processing, which this ISA's target workload (numeric array computation) does not contain; word-addressable numeric machines from the CDC 6600 through the Cray-1 made the same trade. The cost is that sub-word access requires explicit shift-and-mask, which we accept in exchange for a jump field that spans the full address space with no wasted bits and branch targets that need no implicit scaling.

A departure you have *justified* scores better under "Coherence of ISA" (40 pts) than an imitation you cannot explain. **If the team would rather have the realism, take Option B, not C** — commit fully, add the six instructions, pick little-endian, and require aligned `LW`/`SW`. That is a legitimate and more ambitious design; it just costs roughly a day of extra specification and hand-verification that the schedule in Part 3 does not currently have slack for.

## 3. The opcode map is allocated backwards

```
000xxx  ALU       8 slots    ← the most-used class gets the smallest block
001xxx  Memory    8 slots
01xxxx  Other    16 slots    ← bit-manip exotica gets 2x the ALU
10xxxx  FP       16 slots
11xxxx  Control  15 slots
```

Concrete consequences:

- **No immediate-operand instructions exist anywhere in the ISA.** There is no `ADDI`. You cannot increment a loop counter or advance an array pointer without first materializing the constant 1 — and there is no instruction to materialize a constant either (no `LUI`, no `MOVI`). As written, the ISA cannot execute a `for` loop. This is the single most serious gap.
- **No comparison instruction** (`SLT`) and no branch instructions defined yet, so the mechanism by which MAXFINDER decides "is this bigger" is still undetermined.
- **No load or store.** The Memory table is empty; base + displacement addressing (`R(D) ← M(R(S1) + imm)`) is what makes array traversal work and it needs an I-type format with a 16-bit immediate.
- **No arithmetic right shift.** `SRL` and `SRA` differ on negative numbers; you have one undifferentiated "RS".
- Meanwhile `01xxxx` holds `CNTL0`/`CNTT0`/`POP` (count leading zeros / count trailing zeros / popcount), `BITR`/`BYTR` (bit/byte reverse), `SADD`/`SSUB` (saturating arithmetic), `SEXTD`. These are a Zbb-style bit-manipulation extension. They are real instructions in real ISAs, they are each ~1 line in a simulator — and they are each also a benchmark line plus a hand-computed expected result, for zero benefit to either required benchmark.

**`HALT` is assigned twice**: `01-1111` in the Other table and `11-1111` in the Control table, while the header comment says `111111 - Halt`. This is exactly the failure mode of partitioning by person with no single owner of the encoding document.

## 4. Concrete errors in the ALU table (RTL is 40 points)

| Row | Problem |
|---|---|
| `AND: R(D) <- R(S1) && R(S2)` | `&&` is **logical** AND — yields 0 or 1, not a bitwise result. Must be `&`. |
| `OR: R(D) <- R(S1) \|\| R(S2)` | Same — must be `\|`. |
| `RS` | RTL says shift by `R(S2)`, description says "right 1". Contradictory. Also: logical or arithmetic? |
| `LS` | RTL hardcodes `<< 1` while `RS` shifts by a register. Asymmetric for no reason. |
| `NOT` | Redundant if `R0` is hardwired zero and you have `NOR`: `NOT rd, rs` = `NOR rd, rs, R0`. Keep it only as a deliberate convenience, and say so. |

Also unspecified anywhere, and all needed for a correct simulator:
- Which immediates sign-extend vs. zero-extend (per instruction).
- Overflow behavior. **Recommendation: no condition flags at all.** Flags are architectural state that serializes instructions; modern RISC designs (RISC-V, Alpha) omit them and use `SLT` + branch. This is a defensible "modern/fast" claim *and* it removes a whole class of simulator bugs.
- Divide-by-zero and shift-amount ≥ 32 semantics.
- FP format (IEEE-754 binary32), rounding mode, NaN/Inf handling.

## 5. Are 6 opcode bits the right number?

Yes, and keeping a `funct` field is the right call. Use the 6 bits as a **primary** opcode with a `funct` escape for the register-register instructions, not as a flat list of 40.

With 5-bit register fields, a register-register op needs `6 + 5 + 5 + 5 = 21` bits, leaving **11 bits free** in every R-type instruction. Spending primary opcodes on instructions that have 11 unused bits sitting right there is wasteful.

- **Three escapes** (`R-ALU`, `R-FP`, `R-OTHER`) carry about 24 register-register operations through `funct`. A flat design would spend 24 primary opcodes on them; this spends 3.
- The whole ISA then uses about **18 of the 64** primary opcodes, leaving the rest reserved.
- `funct` does **not** reduce the instruction count. It reduces how much *opcode space* the instructions consume, and makes adding a later R-type instruction cost one `funct` value instead of one primary opcode.

**Why keep 6 opcode bits when ~18 would fit in 5.** The spare bit is encoding freedom, not decode speed. It lets opcodes be assigned so class and format fall out of a 2–3-bit prefix, so the ALU control can be shared between R-type and I-type, and so there are reserved codes to move an opcode later if a datapath project shows a control signal on the critical path. It costs nothing, because the I and J formats are already fixed by their 16- and 26-bit fields.

**Honest cost of `funct`:** R-type decode is two steps (opcode picks the class, then `funct` picks the operation) where a flat opcode is one. For the simulator that is trivial; in hardware it is a small extra lookup. Flat is marginally simpler to decode and harder to extend. For this project's goals (extensible, easy to group, designed to be reworked into a datapath later), `funct` is the better trade.

It also lets the existing class blocks survive: they move from primary-opcode ranges into `funct` spaces, so each owner keeps a block (see §2.3).

## 6. Clean up the register file definition

"32 registers – 8 non-addressable" is ambiguous and, on either reading, a liability. Non-addressable architectural state only becomes useful if you add move instructions to get data in and out of it (MIPS needed `MFHI`/`MFLO` for exactly this) — more opcodes, more benchmark lines, no gain here.

**Recommendation:**
- **32 addressable GPRs**, `R0` hardwired to zero. `R0` is free leverage: `MOV rd, rs` = `ADD rd, rs, R0`; `BEQZ rs, L` = `BEQ rs, R0, L`; `NOP` = `ADD R0, R0, R0`; `J` = `JAL` with link to `R0`; `JR` = `JALR` with `rd = R0`.
- `SP` / `RA` / `FP` are **ABI conventions** on specific GPRs (e.g. `R29`/`R31`/`R30`), documented in the spec, enforced by nothing. That is how every modern RISC does it.
- `PC` is the only non-GPR architectural register.

## 7. Two FP decisions that make the 75-point FP MAXFINDER nearly free

**(a) Share one 32-entry register file between integer and FP.** FP instructions interpret the 32 bits they read as IEEE-754 binary32. Consequences: no second register file in the simulator, no `FMV` instructions, `LW`/`SW` move floats with no `FLW`/`FSW`, and the FP MAXFINDER becomes the integer MAXFINDER with four mnemonics changed.
*Honest trade:* real ISAs split the files for register-port and bandwidth reasons, and because FP registers are often wider than GPRs. Say so in the spec — a stated trade-off scores better than an unexamined one.

**(b) FP compares write an integer register, not a flag.** `FLT rd, rs1, rs2` → `R(rd) ← (R(rs1) <f R(rs2)) ? 1 : 0`, then branch with the **existing** `BNE rd, R0, L`. This is the RISC-V approach. The MIPS alternative (an FP condition-code bit plus `BC1T`/`BC1F`) adds hidden state and two more branch instructions to spec, implement, and cover.

## 8. One instruction worth *promoting* from the "Other" block

`CONDMV` (conditional move) — `CMOV rd, rs1, rs2`: `if R(rs2) ≠ 0 then R(rd) ← R(rs1)`.

This lets both MAXFINDERs be written **branchless**: compare, then conditionally move. That is a real, articulable modern-performance argument (no branch to mispredict in the inner loop) that you can write up in the spec, and it costs one instruction. Keep it. Of the rest of the original "Other" block, the plan (§2.4) keeps `CNTL0`/`CNTT0`/`POP`/`BYTR`/`SEXTD` as a small R-OTHER set and drops `BITR`/`SADD`/`SSUB` to optional extras.

---

# PART 2 — RECOMMENDED SPECIFICATION

## 2.1 Global parameters

| Parameter | Value |
|---|---|
| Instruction width | 32 bits, fixed |
| Registers | 32 × 32-bit GPRs, `R0` hardwired to 0; shared integer/FP |
| Special registers | `PC` only |
| Memory | Word-addressable, 26-bit word address, 32 bits per word |
| `.mem` header | `26,32` |
| Sequential PC | `PC ← PC + 1` |
| Branch target | `PC ← PC + 1 + sext(offset16)` (offset in instructions) |
| Jump target | absolute 26-bit word address |
| Condition flags | none |
| FP format | IEEE-754 binary32, round-to-nearest-even |
| Endianness | n/a (no sub-word access) |

The **Memory**, **`.mem` header**, **Sequential PC**, **Branch target**, **Jump target**, and **Endianness** rows are the six that change if the team picks Option B. Everything else in Part 2 — register file, formats, opcode map, FP decisions — is identical either way. **§2.7** covers how sub-word data is handled under Option A; **§2.8** is the complete Option B delta.

## 2.2 Instruction formats — three layouts

```
         31    26 25   21 20   16 15   11 10    6 5     0
R-type  | opcode |  rs1  |  rs2  |  rd   | shamt | funct |   register-register ops
I-type  | opcode |  rs1  |  rt   |        imm16          |   ALU-imm, LW, SW, branches, JALR
J-type  | opcode |              target26                 |   J, JAL: absolute word address
```

Field usage:

- **R-type:** `rs1` and `rs2` are the sources, `rd` the destination. `shamt` holds the shift amount for constant shifts and the sign-bit position for `SEXTD`. `funct` selects the operation. Unused fields are **zero** and ignored.
- **I-type:** `rs1` is the base or first source. `rt` is the *destination* for `ADDI`/`ANDI`/`ORI`/`XORI`/`SLTI`/`LUI`/`LW`/`JALR`, and a *source* for `SW` (the value stored) and the branches (the second compare operand). `imm16` is sign-extended, except for `ANDI`/`ORI`/`XORI`, which zero-extend; branch offsets count instructions: `PC ← PC + 1 + sext(imm16)`.
- **J-type:** `PC ← target26`. `JAL` also writes `R(31) ← PC + 1`.

Stores and branches are not separate formats. They have the *same bit layout* as ALU-immediate; the only difference is whether `rt` is written or read, which is a register-write-enable control signal, not a different way of extracting fields. **The spec should say three formats, not five.**

### Why `rs1`/`rs2` sit where they do

The goal is that **both register-file read addresses are in the same bit positions in every format**: `rs1` is always `[25:21]`, and `rs2`/`rt` is always `[20:16]`. Both reads can start the instant the word arrives, with no mux in front of the register file.

The one place a register address moves is the **write** address: `rd` is `[15:11]` in R-type but `rt` is `[20:16]` in I-type. That is a single mux, selected by "is this an R-type opcode?", on the write-back path, which is the least timing-critical place for it. This is the MIPS arrangement. The alternatives were worse:

- Putting `rd` at the same place in R and I (`[25:21]`) puts the mux on the *read* address instead, because stores and branches would then need their second source at `[25:21]` but R-type has it at `[15:11]`.
- RISC-V avoids every mux by scrambling the immediate bits across the word, which moves the complexity into immediate extraction and the assembler.

Worked examples under this layout:

| Instruction | Fields | Hex |
|---|---|---|
| `ADD R3, R1, R2` | `000000` `00001` `00010` `00011` `00000` `000000` | `0x00221800` |
| `ADDI R3, R1, 5` | `001000` `00001` `00011` `0000000000000101` | `0x20230005` |
| `BEQ R1, R2, +3` | `011000` `00001` `00010` `0000000000000011` | `0x60220003` |
| `SW R3, 4(R1)` | `010001` `00001` `00011` `0000000000000100` | `0x44230004` |

These assume the example opcode values from the grouping in §2.3; they will change if the Oct 3 freeze numbers the classes differently, but the field positions will not.

## 2.3 Opcode map — grouping by class

The 6-bit opcode is split as `class[5:3] | op[2:0]`. The class bits identify the instruction family and (with the format column) tell the decoder the layout in a 2–3-bit compare, never a full 6-bit match. **Class membership below is the recommendation; the exact numbering inside each class and each `funct` space is frozen on Oct 3, not here.** `HALT` stays `111111`, as the team's header comment already said.

| `[5:3]` | Class | Members | Format | Owner |
|---|---|---|---|---|
| `000` | Register-register escapes (use `funct`) | `000000` **R-ALU**, `000001` **R-FP**, `000010` **R-OTHER** | R | Piper (ALU), Eldon (FP), Harrison (Other) |
| `001` | ALU-immediate | `ADDI` `ANDI` `ORI` `XORI` `SLTI` `LUI` | I | Zach |
| `010` | Memory | `LW` `SW` | I | Zach |
| `011` | Branch | `BEQ` `BNE` `BLT` `BGE` | I | Brad |
| `100` | Jump | `J` `JAL` (J-type), `JALR` (I-type) | J, I | Brad |
| `101`, `110` | Reserved | **illegal instruction** — the simulator must error, never silently execute | — | — |
| `111` | System | `111111` = `HALT` | — | Brad |

### What lives in each `funct` space

| Escape | `funct` members | Count |
|---|---|---|
| **R-ALU** | `ADD` `SUB` `AND` `OR` `XOR` `SLT` · `SLL` `SRL` `SRA` (shift by `shamt`) · `SLLV` `SRLV` `SRAV` (shift by `R(rs2)[4:0]`) | 12 |
| **R-FP** | `FADD` `FSUB` `FMUL` `FLT` `FCVTWS` `FCVTSW` | 6 |
| **R-OTHER** | `CMOV` `CNTL0` `CNTT0` `POP` `BYTR` `SEXTD` | 6 |

### Numbering rules for the Oct 3 freeze

These are constraints on the numbering, not the numbering itself:

1. **Share ALU control between R-type and I-type.** Give the ALU a 3-bit `alu_op`. In R-type it is `funct[2:0]`; in ALU-immediate it is `opcode[2:0]`. The same value must mean the same operation in both, so one control path serves both:
   `alu_op = (opcode == R-ALU) ? funct[2:0] : opcode[2:0]`

   | `alu_op` | R-type | I-type |
   |---|---|---|
   | `000` | `ADD` | `ADDI` |
   | `010` | `AND` | `ANDI` |
   | `011` | `OR` | `ORI` |
   | `100` | `XOR` | `XORI` |
   | `101` | `SLT` | `SLTI` |
   | `111` | — | `LUI` |

   `SUB` takes the one remaining arithmetic value, with no immediate form (`ADDI` with a negative immediate covers it).
2. **Shifts get their own `funct` sub-group**, with one bit of `funct` meaning "amount comes from a register rather than `shamt`."
3. **Within a class, keep a single "is this a variant" bit** wherever possible, so unit-level decode is a bit test rather than a table.
4. **Reserve, don't fill.** Unassigned opcodes and `funct` values stay reserved and illegal. The spare codes are what let the datapath projects later move a control-critical opcode without breaking everything else.

### How the decoder reads it

| Question | Answer | Cost |
|---|---|---|
| Which format? | `opcode[5:3]`: `000` → R, `100` → J or I, otherwise I | 3-bit compare |
| Is it `HALT`? | `opcode == 111111` | one AND |
| Is it a branch? | `opcode[5:3] == 011` | 3-bit compare |
| Is it memory? | `opcode[5:3] == 010` | 3-bit compare |
| Which unit executes an R-type? | `opcode[2:0]` (ALU / FP / Other) | 3 bits |
| Which operation? | R-type → `funct`; everything else → `opcode[2:0]` | no re-encode |
| Writes a register? | R, ALU-imm, `LW`, `JAL`, `JALR`: yes. `SW`, branches, `J`, `HALT`: no. | small table |
| Write address | R-type → `[15:11]`; I-type → `[20:16]`; `JAL` → R31 | 1 mux, write side only |

## 2.4 The instruction set — about 40, plus free pseudo-instructions

| Class | Instructions | Count |
|---|---|---|
| R-ALU | `ADD` `SUB` `AND` `OR` `XOR` `SLT` `SLL` `SRL` `SRA` `SLLV` `SRLV` `SRAV` | 12 |
| R-OTHER | `CMOV` `CNTL0` `CNTT0` `POP` `BYTR` `SEXTD` | 6 |
| R-FP | `FADD` `FSUB` `FMUL` `FLT` `FCVTWS` `FCVTSW` | 6 |
| ALU-immediate | `ADDI` `ANDI` `ORI` `XORI` `SLTI` `LUI` | 6 |
| Memory | `LW` `SW` | 2 |
| Branch | `BEQ` `BNE` `BLT` `BGE` | 4 |
| Jump | `J` `JAL` `JALR` | 3 |
| System | `HALT` | 1 |
| | | **40** |

### The "Other" set

| Instruction | RTL | Why it is here |
|---|---|---|
| `CMOV rd, rs1, rs2` | `if R(rs2) ≠ 0 then R(rd) ← R(rs1)` | Makes both MAXFINDERs branchless: `SLT t, max, x` then `CMOV max, x, t` (the FP version swaps `SLT` for `FLT`) |
| `CNTL0 rd, rs1` | `R(rd) ← number of leading zeros of R(rs1)` | Normalization and log2; one-value test (`0x00F00000` → 8) |
| `CNTT0 rd, rs1` | `R(rd) ← number of trailing zeros of R(rs1)` | Finds the lowest set bit |
| `POP rd, rs1` | `R(rd) ← number of 1 bits in R(rs1)` | Hardest of these to emulate in software |
| `BYTR rd, rs1` | `R(rd) ← bytes of R(rs1) reversed` | Byte-order tool under word addressing (`0x11223344` → `0x44332211`) |
| `SEXTD rd, rs1, shamt` | `R(rd) ← sext(R(rs1)[shamt:0])` | `shamt` is the sign-bit position: 7 = byte, 15 = halfword. One instruction covers both |

`CNTL0`, `CNTT0`, `POP`, and `BYTR` are unary: `rs2` is unused (the assembler emits zeros, the decoder ignores it). `CNTL0` and `CNTT0` of zero return 32. `SEXTD` ignores `rs2` and uses `shamt` for the bit position.

### Pseudo-instructions (free — not counted toward benchmark coverage)

The handout requires the coverage benchmark to use every *instruction in the ISA*. An assembler alias that expands into real instructions is not an ISA instruction, so these add no hand-verification burden. They all ride on `R0` = 0:

| Pseudo | Expands to |
|---|---|
| `NOP` | `ADD R0, R0, R0` |
| `MOV rd, rs` | `ADD rd, rs, R0` |
| `NEG rd, rs` | `SUB rd, R0, rs` |
| `NOT rd, rs` | `SUB rd, R0, rs` then `ADDI rd, rd, -1` (`-x - 1`) |
| `LI rd, small` | `ADDI rd, R0, small` |
| `LI rd, big` | `LUI rd, hi` then `ORI rd, rd, lo` |
| `BEQZ` / `BNEZ rs, L` | `BEQ` / `BNE rs, R0, L` |
| `BGT` / `BLE a, b, L` | `BLT` / `BGE b, a, L` (swap operands) |
| `JR rs` | `JALR R0, rs` |
| `RET` | `JALR R0, R31` |

`NOT` caveat worth stating in the spec: `XORI` zero-extends, so `XORI rd, rs, 0xFFFF` flips only the low 16 bits and is **not** a 32-bit NOT.

### What was cut, and the workaround

| Cut | Workaround |
|---|---|
| `NOR` | `XOR` with all-ones, or the `NOT` pseudo |
| `MUL` `DIV` `REM` | shift-and-add; neither MAXFINDER needs them |
| `SLTU` `BLTU` `BGEU` | document that every comparison is **signed** |
| `FDIV` `FNEG` `FEQ` | `FNEG` = `FSUB` from a zero register; `FEQ` = `XOR` of the bit patterns, then `BEQZ` (careful with ±0 and NaN) |
| `BITR` | shift-and-mask loop; least useful of the original bit-manipulation ideas |
| `SADD` `SSUB` | each needs an overflow rule and an edge-case hand-verification; skip unless someone owns the semantics |
| `NOOP` as an instruction | pseudo-instruction above |

## 2.5 Optional extras — only by deliberate swap

The 40 are the target, so anything here costs a benchmark line and a hand-computed result. Add only if an owner takes that on: `MUL`, `DIV`, `SLTU`, `BLTU`, `BGEU`, `FDIV`, `FEQ`, `FNEG`, `EXT`/`EXTS` (bit-field extract, see **§2.7**), `BITR`, `SADD`, `SSUB`. Reserved opcode classes and the large unused `funct` space mean none of these require redesigning the formats.

## 2.6 Semantics that must be written down explicitly

These are the lines that cost points when missing and cause simulator/benchmark mismatches when ambiguous:

1. **Sign extension, per instruction.** `ADDI` / `SLTI` / `LW` / `SW` / all branch offsets **sign-extend**. `ANDI` / `ORI` / `XORI` **zero-extend**. (MIPS convention.)
2. **`LUI` semantics:** `R(rd) ← imm16 << 16`. Document the constant-materialization idiom as **`LUI` + `ORI`** (not `LUI` + `ADDI`) — because `ADDI` sign-extends, `LUI`+`ADDI` requires a `+1` correction to the upper half whenever bit 15 of the low half is set. `ORI` zero-extends, so `LUI`+`ORI` is always correct. This is a classic trap; naming it earns coherence credit.
3. **`R0` writes are discarded.**
4. **Shift amount ≥ 32** — define (mask to 5 bits).
5. **Divide by zero** — not applicable: the ISA has no integer divide. If `DIV` is ever added from §2.5, define the result then.
6. **FP:** operations round to nearest-even and produce a *binary32* result. NaN/Inf propagate per IEEE-754; `FLT` returns 0 when either operand is NaN.
7. **`FCVTWS` / `FCVTSW`** — float→int truncates toward zero; define out-of-range behavior (e.g. saturate) and int→float rounding (nearest-even).
8. **`CNTL0` / `CNTT0` of zero** return 32. **`SEXTD`** sign-extends from bit `shamt`; `shamt` must be 0–31.
9. **Unused R-type fields** (`rs2` on unary ops, `shamt` on non-shift ops) are emitted as zero by the assembler and ignored by the decoder.

## 2.7 Sub-word data under Option A — what it actually costs

First, separate two things that are easy to conflate:

- **Bit-field extraction** happens *inside a register*, after the word is loaded. It is shift-and-mask on every ISA — ARM, x86, and RISC-V all do it this way absent a dedicated instruction. **Addressing granularity is completely irrelevant to it.** Byte addressability buys you nothing here.
- **Sub-word memory access** — getting one byte in or out of memory — is the *only* thing addressing granularity affects.

### The idioms

**Bit field `[pos+len-1 : pos]`, zero-extended** (identical under A and B):
```asm
SLL  rd, rs1, 32-(pos+len)   # shift the field up to the top
SRL  rd, rd,  32-len         # shift it back down to bit 0
```
**Sign-extended**: same, with `SRA` on the second shift. This two-shift pair is the standard idiom and needs no mask register.

**Extract byte `n` (constant), zero-extended** — the `LBU` equivalent:
```asm
LW    r1, 0(rBase)
SRL   r1, r1, 8*n
ANDI  r1, r1, 0xFF
```
**Sign-extended** — the `LB` equivalent, no mask needed:
```asm
LW    r1, 0(rBase)
SLL   r1, r1, 24-8*n
SRA   r1, r1, 24
```

**Insert a byte into a word** — the `SB` equivalent. This is the expensive one, because it is a read-modify-write:
```asm
ANDI  rV, rVal, 0xFF      # isolate the byte
SLL   rV, rV,   8*n       # position it
ADDI  rM, R0,   0xFF      # build the mask
SLL   rM, rM,   8*n
NOR   rM, rM,   R0        # rM = ~rM
LW    r1, 0(rBase)
AND   r1, r1,   rM        # clear the target lane
OR    r1, r1,   rV        # insert
SW    r1, 0(rBase)
```

### Honest accounting

| Operation | Option A | Option B |
|---|---|---|
| Load a word | `LW` — 1 | `LW` — 1 |
| Extract a bit field from a register | 2 | 2 (same) |
| Load byte *n*, constant *n* | 3 | `LB`/`LBU` — 1 |
| Load byte at a **runtime** index | 3, uses `SRLV` (in the ISA) | `ADD` + `LBU` — 2 |
| **Store** a byte into a word | **~9** | `SB` — 1 |

Byte *loads* cost 2 extra instructions. Byte *stores* cost ~8 extra and clobber two scratch registers. That asymmetry is the real argument for byte addressing, and it is exactly what Alpha's `EXTBL`/`INSBL`/`MSKBL` existed to soften.

**Neither MAXFINDER performs a sub-word access of any kind**, and the coverage benchmark only needs one if the ISA defines a sub-word instruction to cover. If Option A is chosen, nothing in the project's required workload pays this cost.

**Runtime-indexed byte access works under Option A** because `SLLV`/`SRLV`/`SRAV` are in the ISA (§2.4); constant-`shamt` shifts alone could not do it. For sign-extending a loaded byte or halfword, `SEXTD` (§2.4) is a one-instruction alternative to the `SLL`+`SRA` pair.

### A better answer than byte addressing, if bit-field work matters

If the team wants cheap bit-field manipulation, the right tool is an *instruction*, not a change in addressing. The R-type format already has the bits: reinterpret `rs2[20:16]` as `len` and `shamt[10:6]` as `pos`, with no change to the bit layout.

```
| opcode | rs1 | len | rd | pos | funct |      EXT rd, rs1, pos, len
```
`EXT`: `R(rd) ← zext( R(rs1)[pos+len-1 : pos] )` — a one-instruction replacement for the two-shift idiom, and `EXTS` for the sign-extending variant. This is precisely ARM's `UBFX`/`SBFX` and the RISC-V bit-manipulation approach, it reuses the existing format exactly, and it is a far stronger "modern ISA" talking point than byte loads. Listed under optional extras (**§2.5**).

## 2.8 If the team chooses Option B — the complete delta

Everything above stays except the following. Nothing here is hard individually; the cost is that there are ~15 separate places to be consistent instead of 0, and six of them are hand-verified benchmark results.

**Global parameters (replacing those six rows):**

| Parameter | Option B value |
|---|---|
| Memory | Byte-addressable, 26-bit byte address (64 MiB), 8 bits per address |
| `.mem` header | `26,8` |
| Endianness | **Little-endian** (pick this; it's MIPS-LE/ARM/RISC-V default and matches x86 dev machines) |
| Sequential PC | `PC ← PC + 4` |
| Branch target | `PC ← PC + 4 + (sext(offset16) << 2)` |
| Jump target | `PC ← (PC+4)[31:28] : target26 : 00` (MIPS-style; reaches a 256 MiB region — **document this limitation**, it's why `JALR` is mandatory, not optional) |
| Alignment | `LW`/`SW` require 4-byte alignment, `LH`/`LHU`/`SH` require 2-byte; misaligned access is an **error that halts the simulator with a diagnostic** (simpler than a trap mechanism, and you have no exception architecture) |

**Six added instructions** — all I-type, all in Zach's Memory class (`010xxx`), which has exactly 6 reserved slots left:

| Mnemonic | RTL | Note |
|---|---|---|
| `LB rd, imm(rs1)` | `R(rd) ← sext(M₈(R(rs1)+sext(imm)))` | signed byte |
| `LBU rd, imm(rs1)` | `R(rd) ← zext(M₈(R(rs1)+sext(imm)))` | unsigned byte |
| `LH rd, imm(rs1)` | `R(rd) ← sext(M₁₆(R(rs1)+sext(imm)))` | signed halfword |
| `LHU rd, imm(rs1)` | `R(rd) ← zext(M₁₆(R(rs1)+sext(imm)))` | unsigned halfword |
| `SB rs2, imm(rs1)` | `M₈(R(rs1)+sext(imm)) ← R(rs2)[7:0]` | low byte only; rest of word untouched |
| `SH rs2, imm(rs1)` | `M₁₆(R(rs1)+sext(imm)) ← R(rs2)[15:0]` | low halfword only |

The ISA becomes **~46 instructions**. The `U` variants are not optional padding — without them you cannot load a byte as an unsigned value, which is the common case.

**Simulator impact:** store memory as a byte-keyed sparse map and compose/decompose words on every access, rather than a word-keyed map. Do this *once* in Zach's memory module behind `read8/read16/read32` + `write8/write16/write32` so the endianness logic exists in exactly one place — if byte-lane assembly leaks into the instruction handlers you will be debugging it all of Oct 15.

**Benchmark impact — budget real time for this:**
- Every `.mem` input and expected-output file is ~4× longer and byte-reversed. Write a small helper script to generate them from a word list; do not hand-author them.
- The expected result of each `SB`/`SH` depends on the *prior* contents of the surrounding word. Give each sub-word store its own scratch word at a known initial value so the expected output is derivable without tracking cross-instruction aliasing.
- Add one explicit misalignment test to confirm the diagnostic fires.

**Schedule impact:** add one day. The realistic way to absorb it is to skip every optional extra (§2.5) and trim R-OTHER to `CMOV` plus the two cheapest unary ops.

---

# PART 3 — PROJECT PLAN

## 3.1 Invert the implied order: start the simulator during benchmark week

The due dates (spec → benchmarks → simulator) tempt the team into hand-tracing a 40-instruction coverage benchmark and hand-computing its expected memory contents, then building the simulator afterward. **That is where this project bleeds points.** Instead, get a rough simulator executing by ~Oct 7 so benchmarks are validated by running them. The expected-memory file is then generated by the simulator and **spot-checked by hand** at a few key addresses, rather than derived entirely by hand.

## 3.2 Schedule

| Dates | Work | Owner |
|---|---|---|
| **Oct 1–2** | **Team vote on addressing: Option A or Option B (§2.8). Not Option C.** This is a hard gate — nothing downstream can be written until it lands, because it sets the `.mem` format, the branch RTL, and the instruction count. Then resolve the rest of Part 1: funct escape, `R0`=0, shared register file, FP-compare-to-GPR, three-format layout (§2.2), final instruction list (§2.4). | all |
| | *If B is chosen:* skip all optional extras, trim R-OTHER to `CMOV` plus the two cheapest unary ops, and shift every date below by one day — meaning the spec work runs Oct 3–5 with no review slack, so the Oct 4 coherence pass becomes non-negotiable rather than nice-to-have. | all |
| **Oct 2–3** | **Freeze the encoding.** Harrison produces the single authoritative opcode/funct table; no one edits it afterward without telling him. | Harrison (**encoding owner**) |
| **Oct 3–4** | Each owner fills Format / RTL / Description / Note for their instructions against the frozen encoding. | Piper, Zach, Brad, Eldon, Harrison |
| **Oct 4** | **Coherence pass.** Two people who did *not* write a section read it: every opcode unique, every format diagram's bits sum to 32, every RTL line updates `PC`, every immediate's extension rule stated. | 2 reviewers |
| **Oct 5** | `submit sarahfrost compArchFA26 ISAspec` | Harrison |
| **Oct 3–7** *(overlaps spec)* | Simulator skeleton: CLI/usage, `.mem` reader/writer, assembly tokenizer, two-pass assembler, decoder, register file, fetch loop, step mode, instruction-count table. `HALT`+`ADDI`+`ADD` working end to end. | 1–2 people |
| **Oct 7–10** | Fill in execute handlers by owner, against the running skeleton. | all |
| **Oct 8–12** | Write the three benchmarks; run them; generate `.mem.out`; hand-verify a handful of addresses per benchmark. | all |
| **Oct 12** | `submit ... ISAbench` | — |
| **Oct 12–17** | Harden: step mode polish, edge cases from §2.6, `.instrCount` format, README. | all |
| **Oct 17–18** | **Test on onyx** from a clean checkout. Exact usage string. Fresh shell. | all |
| **Oct 19** | `submit ... ISASim` | — |

## 3.3 Simulator architecture

**Assemble, then decode — do not interpret the text directly.**

```
assembly text → tokenize → two-pass label resolve → encode to 32-bit words
                                                          ↓
                                              decode → execute → writeback
```

The handout only requires executing assembly, so it is tempting to `switch` on the mnemonic string and skip encoding entirely. Don't. Round-tripping through the real 32-bit encoding:
- **proves the encoding in the spec is actually implementable** — it catches immediates that don't fit in 16 bits, register numbers > 31, and branch offsets out of range, which a string interpreter silently accepts;
- makes the decoder a direct transcription of the format diagrams, so spec and simulator can't drift;
- costs maybe 60 lines.

Support labels in the assembler even though the handout says to pre-resolve them (two-pass is ~10 lines), and submit pre-resolved versions as instructed.

Module split, mapped to the same owners:

| Module | Contents | Owner |
|---|---|---|
| `main` / CLI | usage, `-r`/`-s`, file args, step loop | Harrison |
| `assembler` | tokenize, labels, encode per format | Harrison |
| `decode` | 32-bit word → fields | Harrison |
| `memory` | `.mem` parse, sparse store, `.mem.out` emit | Zach |
| `alu` | R-ALU handlers (Piper); ALU-imm handlers (Zach) | Piper, Zach |
| `branch` | branches, jumps, `PC` update | Brad |
| `fp` | all R-FP handlers | Eldon |
| `other` | R-OTHER handlers (`CMOV`, `CNTL0`, `CNTT0`, `POP`, `BYTR`, `SEXTD`) | Harrison |
| `stats` | per-instruction counters, `.instrCount` emit | whoever finishes first |

## 3.4 Implementation risks to settle early

1. **"Runs on onyx with no modifications to my environment."** Decide language on Oct 2, then *immediately* verify the toolchain on onyx — `python3 --version` or `gcc --version` — before writing much. If Python: **stdlib only, no pip**. If C: **C99, libc only, a Makefile**.
2. **The executable must be invokable exactly as `isaSim -r prog.asm prog.mem`.** For C that's the binary name; for Python it needs a shebang'd, `chmod +x` wrapper named `isaSim` with no extension. Test the literal usage string on onyx, not just locally.
3. **Float32 precision.** Python floats are doubles — every FP result must be round-tripped through `struct.pack('<f', x)` / `unpack` to force binary32, or your simulator's answers won't match a 32-bit machine's. In C, use `float` with `memcpy`-based bit reinterpretation (not pointer casts, which are UB under strict aliasing).
4. **Sparse memory.** "Any address not specified contains all 0's" over a 26-bit space — use a dict/hash map, not a 64M-entry array. Then decide and document what `.mem.out` contains: every address ever written, or every address ever touched. Pick one, say so in the README.
5. **Benchmark data lives in the `.mem` file.** The array for MAXFINDER is memory contents, not immediates. Make sure the `.mem` files include array length and base address at known locations so the benchmark can be written generically.

## 3.5 Benchmarks

1. **Coverage benchmark.** One instruction per ISA entry (all ~40; pseudo-instructions need no coverage), grouped by class, each writing its result to a distinct memory address so the expected `.mem.out` is a readable one-result-per-address table. Write it *after* the simulator runs, generate the expected file, then hand-verify ~8 representative addresses spanning each class.
2. **MAXFINDER (int).** Load length and base from memory, loop with `LW` + `SLT` + `CMOV` (branchless inner loop — then say so in the write-up), store the max.
3. **MAXFINDER (float).** The same program with `FLT` substituted for `SLT`. If the two FP decisions in **Part 1 §7** are adopted (shared register file, FP compare writing a GPR), this is a four-line diff from the integer version — which is the entire reason to adopt them. Under Option B it is still a four-line diff; nothing about the FP path changes with addressing granularity, since both MAXFINDERs use full-word access either way.

---

## Verification

- **Spec (Oct 4):** mechanical checks — no duplicate opcode/funct pair; every format diagram's bit fields sum to 32; every RTL line that isn't `HALT` updates `PC`; every immediate has a stated extension rule; `HALT` appears exactly once.
- **Simulator:** run the coverage benchmark in `-r` mode, diff against the hand-verified expected `.mem.out`. Run the same benchmark in `-s` mode and confirm identical final memory and identical `.instrCount`. Confirm `.instrCount` totals equal the step-mode instruction counter.
- **Assembler:** feed it a deliberately bad program (immediate > 16 bits, register `R32`, branch out of range) and confirm it reports an error rather than silently executing.
- **MAXFINDER:** run against three `.mem` files each — max at the first element, max at the last element, and all-negative values (this catches an initialization bug where max starts at 0 instead of `arr[0]`). For the FP version add a negative-zero and an all-negative case.
- **onyx (Oct 17–18):** fresh clone, fresh shell, run the exact usage string from the handout for all three benchmarks in both modes.
