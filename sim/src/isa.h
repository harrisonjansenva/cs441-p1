#pragma once
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file a representation of our ISA in code.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Simulator
{
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief the size of our memory space and the limits that follow from the instruction encodings.
////////////////////////////////////////////////////////////////////////////////////////////////////
constexpr std::uint32_t ADDR_BITS{26};
constexpr std::uint32_t WORD_BITS{32};
constexpr std::uint32_t MAX_ADDR{(1u << ADDR_BITS) - 1}; // also the largest J-type target
constexpr std::uint32_t MAX_BRANCH_TARGET{0xFFFF};       // branches carry a 16 bit target
constexpr std::uint32_t MAX_PROGRAM_ADDR{0xFFFFF};       // largest instruction address we accept

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief the formats we have available for our instructions.
////////////////////////////////////////////////////////////////////////////////////////////////////
enum class Fmt : std::uint8_t
{
   R,
   I,
   J,
   N // no operands
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief the 5 classifications of instructions for our ISA
////////////////////////////////////////////////////////////////////////////////////////////////////
enum class Instr_type : std::uint8_t
{
   Other,
   Memory,
   ALU,
   FP,
   Control
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Within our opcode formats, we have different shapes that can happen. This outlines those.
/// The operands each shape takes are listed in layout().
////////////////////////////////////////////////////////////////////////////////////////////////////
enum class Shape : std::uint8_t
{
   NONE,           // HALT, NOP
   RD_RS1_RS2,     // ADD rd, rs1, rs2
   RD_RS1,         // INC rd, rs1
   RD_RS1_RS2_RS3, // FFMA rd, rs1, rs2, rs3
   RS1_RS2,        // FLT rs1, rs2 | FEQ rs1, rs2
   RT_RS1_IMM,     // LD, ST, FLW, FSW, ADDI, SUBI
   RT_IMM,         // LDR rt, imm16
   RT_RS1,         // MOV rt, rs1
   BR,             // Branch: BEQ rs1, rs2, target
   TGT,            // JUMP target26
   RS1,            // JUMPR rs1
   RS1_RT          // JLINK rs1, rt
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief the kinds of operand that can be written in an instruction, and so the fields they are
/// encoded into.
////////////////////////////////////////////////////////////////////////////////////////////////////
enum class Operand : std::uint8_t
{
   Rd,       // bits 15:11
   Rs1,      // bits 25:21
   Rs2,      // bits 20:16
   Rt,       // bits 20:16, same field as Rs2 but the register is a destination or store source
   Rs3,      // bits 10:6
   Imm16,    // bits 15:0, signed
   Target16, // bits 15:0, a branch target
   Target26  // bits 25:0, a jump target
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief the ordered list of operands a shape takes in assembly.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct ShapeLayout
{
   std::size_t count;
   std::array<Operand, 4> operands;
};

const ShapeLayout &layout(Shape shape);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief the bit position of the lowest bit of an operand's field.
////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned operandShift(Operand operand);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief true if the operand names a register.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool isRegister(Operand operand);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief every instruction as (mnemonic, opcode, format, shape, type). This is the single place
/// instructions are defined, both the Op enum and the ISA table are generated from it.
////////////////////////////////////////////////////////////////////////////////////////////////////
// clang-format off
#define SIM_OP_LIST(X)                        \
   /* Other */                                \
   X(CLZ,   0x00, R, RD_RS1,         Other)   \
   X(CNTT0, 0x01, R, RD_RS1,         Other)   \
   X(POP,   0x02, R, RD_RS1,         Other)   \
   X(BYTR,  0x03, R, RD_RS1,         Other)   \
   X(SEXTD, 0x04, R, RD_RS1,         Other)   \
   X(NOP,   0x07, N, NONE,           Other)   \
   /* Memory */                               \
   X(LD,    0x08, I, RT_RS1_IMM,     Memory)  \
   X(ST,    0x0A, I, RT_RS1_IMM,     Memory)  \
   X(MOV,   0x0C, I, RT_RS1,         Memory)  \
   X(LDR,   0x0D, I, RT_IMM,         Memory)  \
   /* ALU */                                  \
   X(ADD,   0x10, R, RD_RS1_RS2,     ALU)     \
   X(SUB,   0x11, R, RD_RS1_RS2,     ALU)     \
   X(RSB,   0x12, R, RD_RS1_RS2,     ALU)     \
   X(INC,   0x13, R, RD_RS1,         ALU)     \
   X(DEC,   0x14, R, RD_RS1,         ALU)     \
   X(RS,    0x15, R, RD_RS1,         ALU)     \
   X(LS,    0x16, R, RD_RS1,         ALU)     \
   X(AND,   0x17, R, RD_RS1_RS2,     ALU)     \
   X(OR,    0x18, R, RD_RS1_RS2,     ALU)     \
   X(NAND,  0x19, R, RD_RS1_RS2,     ALU)     \
   X(NOR,   0x1A, R, RD_RS1_RS2,     ALU)     \
   X(XOR,   0x1B, R, RD_RS1_RS2,     ALU)     \
   X(NOT,   0x1C, R, RD_RS1,         ALU)     \
   X(ADDI,  0x1D, I, RT_RS1_IMM,     ALU)     \
   X(SUBI,  0x1E, I, RT_RS1_IMM,     ALU)     \
   X(SFLIP, 0x1F, R, RD_RS1,         ALU)     \
   /* Floating point */                       \
   X(FLW,   0x20, I, RT_RS1_IMM,     FP)      \
   X(FSW,   0x21, I, RT_RS1_IMM,     FP)      \
   X(FADD,  0x22, R, RD_RS1_RS2,     FP)      \
   X(FSUB,  0x23, R, RD_RS1_RS2,     FP)      \
   X(FMULT, 0x24, R, RD_RS1_RS2,     FP)      \
   X(FDIV,  0x25, R, RD_RS1_RS2,     FP)      \
   X(FFMA,  0x26, R, RD_RS1_RS2_RS3, FP)      \
   X(FSQRT, 0x27, R, RD_RS1,         FP)      \
   X(FNEG,  0x28, R, RD_RS1,         FP)      \
   X(FABS,  0x29, R, RD_RS1,         FP)      \
   X(FLT,   0x2A, R, RS1_RS2,        FP)      \
   X(FEQ,   0x2B, R, RS1_RS2,        FP)      \
   X(FMAX,  0x2C, R, RD_RS1_RS2,     FP)      \
   X(FTOIW, 0x2D, R, RD_RS1,         FP)      \
   X(IWTOF, 0x2E, R, RD_RS1,         FP)      \
   X(FMOV,  0x2F, R, RD_RS1,         FP)      \
   /* Control */                              \
   X(BEQ,   0x30, I, BR,             Control) \
   X(BLT,   0x31, I, BR,             Control) \
   X(BLE,   0x32, I, BR,             Control) \
   X(BGT,   0x33, I, BR,             Control) \
   X(BGE,   0x34, I, BR,             Control) \
   X(BNE,   0x35, I, BR,             Control) \
   X(JUMP,  0x36, J, TGT,            Control) \
   X(JUMPR, 0x37, I, RS1,            Control) \
   X(JLINK, 0x38, I, RS1_RT,         Control) \
   X(HALT,  0x3F, N, NONE,           Control)
// clang-format on

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief enum for all the instructions. ILLEGAL is not an instruction, it marks the end of the
/// list.
////////////////////////////////////////////////////////////////////////////////////////////////////
#define SIM_OP_ENUM(name, opcode, fmt, shape, type) name,
enum class Op : std::uint8_t
{
   SIM_OP_LIST(SIM_OP_ENUM) ILLEGAL
};
#undef SIM_OP_ENUM

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief easy way to show how many ops we have available
////////////////////////////////////////////////////////////////////////////////////////////////////
constexpr std::size_t NUM_OPS{static_cast<std::size_t>(Op::ILLEGAL)};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief metadata for each instruction
////////////////////////////////////////////////////////////////////////////////////////////////////
struct InstrDesc
{
   const char *name;
   std::uint8_t opcode;
   Fmt fmt;
   Shape shape;
   Instr_type type;
};

extern const InstrDesc ISA[NUM_OPS];

const InstrDesc &desc(Op op);
const char *typeName(Instr_type type);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Gives us the option to look up instruction by opcode or mnemonic
////////////////////////////////////////////////////////////////////////////////////////////////////
Op fromOpcode(std::uint32_t opcode);
Op fromMnemonic(const std::string &upper);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Extractors for the instruction words
////////////////////////////////////////////////////////////////////////////////////////////////////
inline std::uint32_t fOpcode(std::uint32_t w)
{
   return (w >> 26) & 0x3F;
}
inline std::uint32_t fRs1(std::uint32_t w)
{
   return (w >> 21) & 0x1F;
}
inline std::uint32_t fRs2(std::uint32_t w)
{
   return (w >> 16) & 0x1F;
} // also rt
inline std::uint32_t fRd(std::uint32_t w)
{
   return (w >> 11) & 0x1F;
}
inline std::uint32_t fShamt(std::uint32_t w)
{
   return (w >> 6) & 0x1F;
} // also rs3 for FFMA
inline std::uint32_t fImm16(std::uint32_t w)
{
   return w & 0xFFFF;
}
inline std::uint32_t fTarget26(std::uint32_t w)
{
   return w & MAX_ADDR;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Disassemble the instruction word into a string
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string disassemble(std::uint32_t word);

} // namespace Simulator
