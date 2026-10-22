/**
 * @file isa.h
 * @brief Instruction set definition shared by every part of the simulator.
 *
 * The instruction names, opcodes, decoder tables and per-instruction counters
 * all come from the ISA_LIST table below. Adding an instruction that reuses an
 * existing Shape takes one row there and one `case` in Cpu::run(). A new Shape
 * also needs a case in decode() (cpu.cpp), operandCount() and the encoder
 * (assembler.cpp), and disassemble() (isa.cpp).
 */
#pragma once
#include <array>
#include <cstdint>
#include <string>

/** @brief Instruction class, i.e. the group an opcode belongs to. */
enum class Cls : uint8_t
{
   Other,
   Memory,
   ALU,
   FP,
   Control
};

/**
 * @brief Which assembly operands an instruction takes and which instruction
 *        fields they land in.
 */
enum class Shape : uint8_t
{
   NONE,           ///< HALT, NOP
   RD_RS1_RS2,     ///< ADD rd, rs1, rs2
   RD_RS1,         ///< INC rd, rs1
   RD_RS1_RS2_RS3, ///< FFMA rd, rs1, rs2, rs3 (rs3 lives in the shamt field)
   RS1_RS2,        ///< FLT rs1, rs2 and FEQ rs1, rs2 (result goes to FR)
   RT_RS1_IMM,     ///< LD/ST/FLW/FSW/ADDI/SUBI rt, rs1, imm16 (sign-extended)
   RT_RS1,         ///< MOV rt, rs1
   RT_IMM,         ///< LDR rt, imm16
   BR,             ///< BEQ rs1, rt, target (absolute instruction address)
   TGT,            ///< JUMP target (absolute, 26 bits)
   RS1,            ///< JUMPR rs1
   RS1_RT          ///< JLINK rs1, rt
};

/**
 * @brief X-macro table with one row per instruction.
 *
 * Each row is `X(name, 6-bit opcode, operand shape, class)`. Rows
 * must stay grouped by class so the instruction-count report prints one
 * heading per class.
 */
#define ISA_LIST(X)                                                                                \
   /* Other 000xxx */                                                                              \
   X(CLZ, 0x00, RD_RS1, Other)                                                                     \
   X(CNTT0, 0x01, RD_RS1, Other)                                                                   \
   X(POP, 0x02, RD_RS1, Other)                                                                     \
   X(BYTR, 0x03, RD_RS1, Other)                                                                    \
   X(SEXTD, 0x04, RD_RS1, Other)                                                                   \
   X(NOP, 0x07, NONE, Other)                                                                       \
   /* Memory 001xxx */                                                                             \
   X(LD, 0x08, RT_RS1_IMM, Memory)                                                                 \
   X(ST, 0x0A, RT_RS1_IMM, Memory)                                                                 \
   X(MOV, 0x0C, RT_RS1, Memory)                                                                    \
   X(LDR, 0x0D, RT_IMM, Memory)                                                                    \
   /* ALU 01xxxx */                                                                                \
   X(ADD, 0x10, RD_RS1_RS2, ALU)                                                                   \
   X(SUB, 0x11, RD_RS1_RS2, ALU)                                                                   \
   X(RSB, 0x12, RD_RS1_RS2, ALU)                                                                   \
   X(INC, 0x13, RD_RS1, ALU)                                                                       \
   X(DEC, 0x14, RD_RS1, ALU)                                                                       \
   X(RS, 0x15, RD_RS1, ALU)                                                                        \
   X(LS, 0x16, RD_RS1, ALU)                                                                        \
   X(AND, 0x17, RD_RS1_RS2, ALU)                                                                   \
   X(OR, 0x18, RD_RS1_RS2, ALU)                                                                    \
   X(NAND, 0x19, RD_RS1_RS2, ALU)                                                                  \
   X(NOR, 0x1A, RD_RS1_RS2, ALU)                                                                   \
   X(XOR, 0x1B, RD_RS1_RS2, ALU)                                                                   \
   X(NOT, 0x1C, RD_RS1, ALU)                                                                       \
   X(ADDI, 0x1D, RT_RS1_IMM, ALU)                                                                  \
   X(SUBI, 0x1E, RT_RS1_IMM, ALU)                                                                  \
   X(SFLIP, 0x1F, RD_RS1, ALU)                                                                     \
   /* Floating point 10xxxx */                                                                     \
   X(FLW, 0x20, RT_RS1_IMM, FP)                                                                    \
   X(FSW, 0x21, RT_RS1_IMM, FP)                                                                    \
   X(FADD, 0x22, RD_RS1_RS2, FP)                                                                   \
   X(FSUB, 0x23, RD_RS1_RS2, FP)                                                                   \
   X(FMULT, 0x24, RD_RS1_RS2, FP)                                                                  \
   X(FDIV, 0x25, RD_RS1_RS2, FP)                                                                   \
   X(FFMA, 0x26, RD_RS1_RS2_RS3, FP)                                                               \
   X(FSQRT, 0x27, RD_RS1, FP)                                                                      \
   X(FNEG, 0x28, RD_RS1, FP)                                                                       \
   X(FABS, 0x29, RD_RS1, FP)                                                                       \
   X(FLT, 0x2A, RS1_RS2, FP)                                                                       \
   X(FEQ, 0x2B, RS1_RS2, FP)                                                                       \
   X(FMAX, 0x2C, RD_RS1_RS2, FP)                                                                   \
   X(FTOIW, 0x2D, RD_RS1, FP)                                                                      \
   X(IWTOF, 0x2E, RD_RS1, FP)                                                                      \
   X(FMOV, 0x2F, RD_RS1, FP)                                                                       \
   /* Control 11xxxx */                                                                            \
   X(BEQ, 0x30, BR, Control)                                                                       \
   X(BLT, 0x31, BR, Control)                                                                       \
   X(BLE, 0x32, BR, Control)                                                                       \
   X(BGT, 0x33, BR, Control)                                                                       \
   X(BGE, 0x34, BR, Control)                                                                       \
   X(BNE, 0x35, BR, Control)                                                                       \
   X(JUMP, 0x36, TGT, Control)                                                                     \
   X(JUMPR, 0x37, RS1, Control)                                                                    \
   X(JLINK, 0x38, RS1_RT, Control)                                                                 \
   X(HALT, 0x3F, NONE, Control)

/** @brief One enumerator per instruction, in ISA_LIST order. */
enum class Op : uint8_t
{
#define X(name, opc, shape, cls) name,
   ISA_LIST(X)
#undef X
   ILLEGAL, ///< no such instruction; its value is also the instruction count
};

/** @brief Number of real instructions (excludes Op::ILLEGAL). */
constexpr int NUM_OPS = static_cast<int>(Op::ILLEGAL);

/** @brief One row of the ISA table, generated from ISA_LIST. */
struct InstrDesc
{
   const char *name; ///< upper-case mnemonic
   uint8_t opcode;   ///< 6-bit opcode
   Shape shape;
   Cls cls;
};

inline constexpr InstrDesc ISA[] = {
#define X(name, opc, shape, cls) {#name, opc, Shape::shape, Cls::cls},
    ISA_LIST(X)
#undef X
};

/**
 * @brief Look up the table row for an instruction.
 * @param op A real instruction; Op::ILLEGAL has no row.
 */
inline const InstrDesc &desc(Op op)
{
   return ISA[static_cast<int>(op)];
}

/**
 * @brief Human-readable name of an instruction class.
 * @param c Instruction class.
 * @return Name used in the instruction-count report.
 */
const char *className(Cls c);

/**
 * @brief Map a 6-bit opcode to its instruction.
 * @param opcode6 Opcode field of an instruction word.
 * @return The instruction, or Op::ILLEGAL if the opcode is unassigned.
 */
Op opFromOpcode(uint32_t opcode6);

/**
 * @brief Map an assembly mnemonic to its instruction.
 * @param upper Upper-case mnemonic. The aliases CNTL0 and NOOP are accepted.
 * @return The instruction, or Op::ILLEGAL if the mnemonic is unknown.
 */
Op opFromMnemonic(const std::string &upper);

/**
 * @name Instruction word fields
 * Layouts from the ISA spec:
 * - R: opcode[31:26] rs1[25:21] rs2[20:16] rd[15:11] shamt[10:6] funct[5:0]
 * - I: opcode[31:26] rs1[25:21] rt[20:16] imm16[15:0]
 * - J: opcode[31:26] target26[25:0]
 * @{
 */
inline uint32_t fOpcode(uint32_t w)
{
   return w >> 26;
}
inline uint32_t fRs1(uint32_t w)
{
   return (w >> 21) & 31;
}
/** Also the `rt` field of I-type words. */
inline uint32_t fRs2(uint32_t w)
{
   return (w >> 16) & 31;
}
inline uint32_t fRd(uint32_t w)
{
   return (w >> 11) & 31;
}
inline uint32_t fShamt(uint32_t w)
{
   return (w >> 6) & 31;
}
inline uint32_t fImm16(uint32_t w)
{
   return w & 0xFFFF;
}
inline uint32_t fTarget26(uint32_t w)
{
   return w & 0x3FFFFFF;
}
/** @} */

/**
 * @name Instruction word encoders
 * Inverses of the field extractors above.
 * @{
 */
inline uint32_t makeR(uint32_t opcode, uint32_t rs1, uint32_t rs2, uint32_t rd, uint32_t shamt = 0)
{
   return opcode << 26 | rs1 << 21 | rs2 << 16 | rd << 11 | shamt << 6;
}
inline uint32_t makeI(uint32_t opcode, uint32_t rs1, uint32_t rt, uint32_t imm16)
{
   return opcode << 26 | rs1 << 21 | rt << 16 | (imm16 & 0xFFFF);
}
inline uint32_t makeJ(uint32_t opcode, uint32_t target26)
{
   return opcode << 26 | (target26 & 0x3FFFFFF);
}
/** @} */

/**
 * @brief Disassemble one instruction word.
 * @param word Raw 32-bit instruction.
 * @return Assembly text, or `.word 0x...` if the opcode is unassigned.
 */
std::string disassemble(uint32_t word);
