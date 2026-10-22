#pragma once

#include <cstdint>
#include <string>

// Instruction formats
enum class Fmt : uint8_t
{
   R,
   I,
   J,
   N // No operands (HALT, NOP)
};

// Instruction classes
enum class Cls : uint8_t
{
   Other,
   Memory,
   ALU,
   FP,
   Control
};

// Operand layout shapes for assembler/disassembler
enum class Shape : uint8_t
{
   NONE,           // HALT, NOP
   RD_RS1_RS2,     // ADD rd, rs1, rs2
   RD_RS1,         // INC rd, rs1
   RD_RS1_RS2_RS3, // FFMA rd, rs1, rs2, rs3
   RS1_RS2,        // FLT rs1, rs2 | FEQ rs1, rs2
   RT_RS1_IMM,     // LD, ST, FLW, FSW, ADDI, SUBI
   RT_RS1,         // MOV rt, rs1
   RT_IMM,         // LDR rt, imm16
   BR,             // Branch: BEQ rs1, rt, target
   TGT,            // JUMP target26
   RS1,            // JUMPR rs1
   RS1_RT          // JLINK rs1, rt
};

// Instruction enum
enum class Op : uint8_t
{
   // Other (0x00 - 0x07)
   CLZ = 0,
   CNTT0,
   POP,
   BYTR,
   SEXTD,
   NOP,

   // Memory (0x08 - 0x0F)
   LD,
   ST,
   MOV,
   LDR,

   // ALU (0x10 - 0x1F)
   ADD,
   SUB,
   RSB,
   INC,
   DEC,
   RS,
   LS,
   AND,
   OR,
   NAND,
   NOR,
   XOR,
   NOT,
   ADDI,
   SUBI,
   SFLIP,

   // Floating Point (0x20 - 0x2F)
   FLW,
   FSW,
   FADD,
   FSUB,
   FMULT,
   FDIV,
   FFMA,
   FSQRT,
   FNEG,
   FABS,
   FLT,
   FEQ,
   FMAX,
   FTOIW,
   IWTOF,
   FMOV,

   // Control (0x30 - 0x3F)
   BEQ,
   BLT,
   BLE,
   BGT,
   BGE,
   BNE,
   JUMP,
   JUMPR,
   JLINK,
   HALT,

   ILLEGAL
};

constexpr int NUM_OPS = static_cast<int>(Op::ILLEGAL);

// Metadata for each instruction
struct InstrDesc
{
   const char *name;
   uint8_t opcode;
   Fmt fmt;
   Shape shape;
   Cls cls;
};

extern const InstrDesc ISA[NUM_OPS];

const InstrDesc &desc(Op op);
const char *className(Cls c);

// Look up Op by 6-bit opcode or uppercase mnemonic
Op opFromOpcode(uint32_t opcode6);
Op opFromMnemonic(const std::string &upper);

// Field extractors for 32-bit instruction words
inline uint32_t fOpcode(uint32_t w)
{
   return (w >> 26) & 0x3F;
}
inline uint32_t fRs1(uint32_t w)
{
   return (w >> 21) & 0x1F;
}
inline uint32_t fRs2(uint32_t w)
{
   return (w >> 16) & 0x1F;
} // also rt
inline uint32_t fRd(uint32_t w)
{
   return (w >> 11) & 0x1F;
}
inline uint32_t fShamt(uint32_t w)
{
   return (w >> 6) & 0x1F;
} // also rs3 for FFMA
inline uint32_t fImm16(uint32_t w)
{
   return w & 0xFFFF;
}
inline uint32_t fTarget26(uint32_t w)
{
   return w & 0x3FFFFFF;
}

// Disassemble instruction word to string
std::string disassemble(uint32_t word);
