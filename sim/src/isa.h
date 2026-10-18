// isa.h - the single source of truth for the instruction set.
//
// Everything that knows about an instruction (assembler, encoder, decoder,
// disassembler, per-instruction counters) is driven by the ISA_LIST table
// below. Changing or adding an instruction = one row here + one `case` in
// cpu.cpp.
#pragma once
#include <array>
#include <cstdint>
#include <string>

enum class Fmt : uint8_t
{
   R,
   I,
   J,
   N
}; // N = opcode only (no operand fields)
enum class Cls : uint8_t
{
   Other,
   Memory,
   ALU,
   FP,
   Control
};

// Operand shapes: which assembly operands an instruction takes and which
// instruction fields they land in.
enum class Shape : uint8_t
{
   NONE,           // HALT NOP RET
   RD_RS1_RS2,     // ADD rd, rs1, rs2
   RD_RS1,         // INC rd, rs1
   RD,             // FILL rd | CLR rd | IN rd | OUT rd (rd field is the register)
   RD_RS1_RS2_RS3, // FFMA rd, rs1, rs2, rs3 (rs3 lives in the shamt field)
   RS1_RS2,        // FLT rs1, rs2 | FEQ rs1, rs2 (result goes to FR)
   RT_RS1_IMM,     // LD/ST/FLW/FSW/ADDI/SUBI rt, rs1, imm16 (sign-extended)
   RT_RS1,         // MOV rt, rs1
   RT_IMM,         // LDR rt, imm16
   BR,             // BEQ rs1, rt, target (target = absolute instruction address)
   TGT,            // JUMP/BEXT target (absolute, 26 bits)
   RS1,            // JUMPR rs1
   RS1_RT          // JLINK rs1, rt
};

// X(name, 6-bit opcode, format, operand shape, class)
#define ISA_LIST(X)                                                                                \
   /* Other 000xxx */                                                                              \
   X(CLZ, 0x00, R, RD_RS1, Other)                                                                  \
   X(CNTT0, 0x01, R, RD_RS1, Other)                                                                \
   X(POP, 0x02, R, RD_RS1, Other)                                                                  \
   X(BYTR, 0x03, R, RD_RS1, Other)                                                                 \
   X(SEXTD, 0x04, R, RD_RS1, Other)                                                                \
   X(MOVGT, 0x05, R, RD_RS1_RS2, Other)                                                            \
   X(MOVLT, 0x06, R, RD_RS1_RS2, Other)                                                            \
   X(NOP, 0x07, N, NONE, Other)                                                                    \
   /* Memory 001xxx */                                                                             \
   X(LD, 0x08, I, RT_RS1_IMM, Memory)                                                              \
   X(ST, 0x0A, I, RT_RS1_IMM, Memory)                                                              \
   X(MOV, 0x0C, I, RT_RS1, Memory)                                                                 \
   X(LDR, 0x0D, I, RT_IMM, Memory)                                                                 \
   /* ALU 01xxxx */                                                                                \
   X(ADD, 0x10, R, RD_RS1_RS2, ALU)                                                                \
   X(SUB, 0x11, R, RD_RS1_RS2, ALU)                                                                \
   X(RSB, 0x12, R, RD_RS1_RS2, ALU)                                                                \
   X(INC, 0x13, R, RD_RS1, ALU)                                                                    \
   X(DEC, 0x14, R, RD_RS1, ALU)                                                                    \
   X(RS, 0x15, R, RD_RS1, ALU)                                                                     \
   X(LS, 0x16, R, RD_RS1, ALU)                                                                     \
   X(AND, 0x17, R, RD_RS1_RS2, ALU)                                                                \
   X(OR, 0x18, R, RD_RS1_RS2, ALU)                                                                 \
   X(NAND, 0x19, R, RD_RS1_RS2, ALU)                                                               \
   X(NOR, 0x1A, R, RD_RS1_RS2, ALU)                                                                \
   X(XOR, 0x1B, R, RD_RS1_RS2, ALU)                                                                \
   X(NOT, 0x1C, R, RD_RS1, ALU)                                                                    \
   X(ADDI, 0x1D, I, RT_RS1_IMM, ALU)                                                               \
   X(SUBI, 0x1E, I, RT_RS1_IMM, ALU)                                                               \
   X(SFLIP, 0x1F, R, RD_RS1, ALU)                                                                  \
   /* Floating point 10xxxx */                                                                     \
   X(FLW, 0x20, I, RT_RS1_IMM, FP)                                                                 \
   X(FSW, 0x21, I, RT_RS1_IMM, FP)                                                                 \
   X(FADD, 0x22, R, RD_RS1_RS2, FP)                                                                \
   X(FSUB, 0x23, R, RD_RS1_RS2, FP)                                                                \
   X(FMULT, 0x24, R, RD_RS1_RS2, FP)                                                               \
   X(FDIV, 0x25, R, RD_RS1_RS2, FP)                                                                \
   X(FFMA, 0x26, R, RD_RS1_RS2_RS3, FP)                                                            \
   X(FSQRT, 0x27, R, RD_RS1, FP)                                                                   \
   X(FNEG, 0x28, R, RD_RS1, FP)                                                                    \
   X(FABS, 0x29, R, RD_RS1, FP)                                                                    \
   X(FLT, 0x2A, R, RS1_RS2, FP)                                                                    \
   X(FEQ, 0x2B, R, RS1_RS2, FP)                                                                    \
   X(FMAX, 0x2C, R, RD_RS1_RS2, FP)                                                                \
   X(FTOIW, 0x2D, R, RD_RS1, FP)                                                                   \
   X(IWTOF, 0x2E, R, RD_RS1, FP)                                                                   \
   X(FMOV, 0x2F, R, RD_RS1, FP)                                                                    \
   /* Control 11xxxx */                                                                            \
   X(BEQ, 0x30, I, BR, Control)                                                                    \
   X(BLT, 0x31, I, BR, Control)                                                                    \
   X(BLE, 0x32, I, BR, Control)                                                                    \
   X(BGT, 0x33, I, BR, Control)                                                                    \
   X(BGE, 0x34, I, BR, Control)                                                                    \
   X(BNE, 0x35, I, BR, Control)                                                                    \
   X(JUMP, 0x36, J, TGT, Control)                                                                  \
   X(JUMPR, 0x37, I, RS1, Control)                                                                 \
   X(JLINK, 0x38, I, RS1_RT, Control)                                                              \
   X(BEXT, 0x39, J, TGT, Control)                                                                  \
   X(IN, 0x3A, R, RD, Control)                                                                     \
   X(OUT, 0x3B, R, RD, Control)                                                                    \
   X(RET, 0x3C, N, NONE, Control)                                                                  \
   X(FILL, 0x3D, R, RD, Control)                                                                   \
   X(CLR, 0x3E, R, RD, Control)                                                                    \
   X(HALT, 0x3F, N, NONE, Control)

enum class Op : uint8_t
{
#define X(name, opc, fmt, shape, cls) name,
   ISA_LIST(X)
#undef X
   ILLEGAL, // also the number of real instructions
};

constexpr int NUM_OPS = static_cast<int>(Op::ILLEGAL);

struct InstrDesc
{
   const char *name;
   uint8_t opcode;
   Fmt fmt;
   Shape shape;
   Cls cls;
};

inline constexpr InstrDesc ISA[] = {
#define X(name, opc, fmt, shape, cls) {#name, opc, Fmt::fmt, Shape::shape, Cls::cls},
    ISA_LIST(X)
#undef X
};

inline const InstrDesc &desc(Op op)
{
   return ISA[static_cast<int>(op)];
}

const char *className(Cls c);

// opcode(6 bit) -> Op, or Op::ILLEGAL
Op opFromOpcode(uint32_t opcode6);
// mnemonic (upper-case; accepts aliases CNTL0 / NOOP) -> Op, or Op::ILLEGAL
Op opFromMnemonic(const std::string &upper);

// 32-bit instruction word fields (R/I/J layouts from the spec):
//   R: opcode[31:26] rs1[25:21] rs2[20:16] rd[15:11] shamt[10:6] funct[5:0]
//   I: opcode[31:26] rs1[25:21] rt[20:16]  imm16[15:0]
//   J: opcode[31:26] target26[25:0]
inline uint32_t fOpcode(uint32_t w)
{
   return w >> 26;
}
inline uint32_t fRs1(uint32_t w)
{
   return (w >> 21) & 31;
}
inline uint32_t fRs2(uint32_t w)
{
   return (w >> 16) & 31;
} // also rt
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

std::string disassemble(uint32_t word);
