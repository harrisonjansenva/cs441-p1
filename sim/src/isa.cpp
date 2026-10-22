#include "isa.h"

#include <cstdio>
#include <string>

// Table of all 52 supported instructions
const InstrDesc ISA[NUM_OPS] = {// Other (0x00 - 0x07)
                                {"CLZ", 0x00, Fmt::R, Shape::RD_RS1, Cls::Other},
                                {"CNTT0", 0x01, Fmt::R, Shape::RD_RS1, Cls::Other},
                                {"POP", 0x02, Fmt::R, Shape::RD_RS1, Cls::Other},
                                {"BYTR", 0x03, Fmt::R, Shape::RD_RS1, Cls::Other},
                                {"SEXTD", 0x04, Fmt::R, Shape::RD_RS1, Cls::Other},
                                {"NOP", 0x07, Fmt::N, Shape::NONE, Cls::Other},

                                // Memory (0x08 - 0x0F)
                                {"LD", 0x08, Fmt::I, Shape::RT_RS1_IMM, Cls::Memory},
                                {"ST", 0x0A, Fmt::I, Shape::RT_RS1_IMM, Cls::Memory},
                                {"MOV", 0x0C, Fmt::I, Shape::RD_RS1, Cls::Memory},
                                {"LDR", 0x0D, Fmt::I, Shape::RT_IMM, Cls::Memory},

                                // ALU (0x10 - 0x1F)
                                {"ADD", 0x10, Fmt::R, Shape::RD_RS1_RS2, Cls::ALU},
                                {"SUB", 0x11, Fmt::R, Shape::RD_RS1_RS2, Cls::ALU},
                                {"RSB", 0x12, Fmt::R, Shape::RD_RS1_RS2, Cls::ALU},
                                {"INC", 0x13, Fmt::R, Shape::RD_RS1, Cls::ALU},
                                {"DEC", 0x14, Fmt::R, Shape::RD_RS1, Cls::ALU},
                                {"RS", 0x15, Fmt::R, Shape::RD_RS1, Cls::ALU},
                                {"LS", 0x16, Fmt::R, Shape::RD_RS1, Cls::ALU},
                                {"AND", 0x17, Fmt::R, Shape::RD_RS1_RS2, Cls::ALU},
                                {"OR", 0x18, Fmt::R, Shape::RD_RS1_RS2, Cls::ALU},
                                {"NAND", 0x19, Fmt::R, Shape::RD_RS1_RS2, Cls::ALU},
                                {"NOR", 0x1A, Fmt::R, Shape::RD_RS1_RS2, Cls::ALU},
                                {"XOR", 0x1B, Fmt::R, Shape::RD_RS1_RS2, Cls::ALU},
                                {"NOT", 0x1C, Fmt::R, Shape::RD_RS1, Cls::ALU},
                                {"ADDI", 0x1D, Fmt::I, Shape::RT_RS1_IMM, Cls::ALU},
                                {"SUBI", 0x1E, Fmt::I, Shape::RT_RS1_IMM, Cls::ALU},
                                {"SFLIP", 0x1F, Fmt::R, Shape::RD_RS1, Cls::ALU},

                                // Floating Point (0x20 - 0x2F)
                                {"FLW", 0x20, Fmt::I, Shape::RT_RS1_IMM, Cls::FP},
                                {"FSW", 0x21, Fmt::I, Shape::RT_RS1_IMM, Cls::FP},
                                {"FADD", 0x22, Fmt::R, Shape::RD_RS1_RS2, Cls::FP},
                                {"FSUB", 0x23, Fmt::R, Shape::RD_RS1_RS2, Cls::FP},
                                {"FMULT", 0x24, Fmt::R, Shape::RD_RS1_RS2, Cls::FP},
                                {"FDIV", 0x25, Fmt::R, Shape::RD_RS1_RS2, Cls::FP},
                                {"FFMA", 0x26, Fmt::R, Shape::RD_RS1_RS2_RS3, Cls::FP},
                                {"FSQRT", 0x27, Fmt::R, Shape::RD_RS1, Cls::FP},
                                {"FNEG", 0x28, Fmt::R, Shape::RD_RS1, Cls::FP},
                                {"FABS", 0x29, Fmt::R, Shape::RD_RS1, Cls::FP},
                                {"FLT", 0x2A, Fmt::R, Shape::RS1_RS2, Cls::FP},
                                {"FEQ", 0x2B, Fmt::R, Shape::RS1_RS2, Cls::FP},
                                {"FMAX", 0x2C, Fmt::R, Shape::RD_RS1_RS2, Cls::FP},
                                {"FTOIW", 0x2D, Fmt::R, Shape::RD_RS1, Cls::FP},
                                {"IWTOF", 0x2E, Fmt::R, Shape::RD_RS1, Cls::FP},
                                {"FMOV", 0x2F, Fmt::R, Shape::RD_RS1, Cls::FP},

                                // Control (0x30 - 0x3F)
                                {"BEQ", 0x30, Fmt::I, Shape::BR, Cls::Control},
                                {"BLT", 0x31, Fmt::I, Shape::BR, Cls::Control},
                                {"BLE", 0x32, Fmt::I, Shape::BR, Cls::Control},
                                {"BGT", 0x33, Fmt::I, Shape::BR, Cls::Control},
                                {"BGE", 0x34, Fmt::I, Shape::BR, Cls::Control},
                                {"BNE", 0x35, Fmt::I, Shape::BR, Cls::Control},
                                {"JUMP", 0x36, Fmt::J, Shape::TGT, Cls::Control},
                                {"JUMPR", 0x37, Fmt::I, Shape::RS1, Cls::Control},
                                {"JLINK", 0x38, Fmt::I, Shape::RS1_RT, Cls::Control},
                                {"HALT", 0x3F, Fmt::N, Shape::NONE, Cls::Control}};

const InstrDesc &desc(Op op)
{
   static const InstrDesc illegalDesc = {"ILLEGAL", 0xFF, Fmt::N, Shape::NONE, Cls::Other};
   int idx = static_cast<int>(op);
   if (idx >= 0 && idx < NUM_OPS)
   {
      return ISA[idx];
   }
   return illegalDesc;
}

const char *className(Cls c)
{
   switch (c)
   {
   case Cls::Other:
      return "Other";
   case Cls::Memory:
      return "Memory";
   case Cls::ALU:
      return "ALU";
   case Cls::FP:
      return "Floating Point";
   case Cls::Control:
      return "Control";
   }
   return "?";
}

Op opFromOpcode(uint32_t opcode6)
{
   if (opcode6 >= 64)
   {
      return Op::ILLEGAL;
   }
   for (int i = 0; i < NUM_OPS; i++)
   {
      if (ISA[i].opcode == opcode6)
      {
         return static_cast<Op>(i);
      }
   }
   return Op::ILLEGAL;
}

Op opFromMnemonic(const std::string &upper)
{
   for (int i = 0; i < NUM_OPS; i++)
   {
      if (upper == ISA[i].name)
      {
         return static_cast<Op>(i);
      }
   }
   // Common aliases from earlier drafts
   if (upper == "CNTL0")
      return Op::CLZ;
   if (upper == "NOOP")
      return Op::NOP;
   return Op::ILLEGAL;
}

std::string disassemble(uint32_t w)
{
   Op op = opFromOpcode(fOpcode(w));
   if (op == Op::ILLEGAL)
   {
      char b[48];
      std::snprintf(b, sizeof(b), ".word 0x%08X", w);
      return b;
   }

   const InstrDesc &d = desc(op);
   char b[96];
   int16_t simm = static_cast<int16_t>(fImm16(w));

   switch (d.shape)
   {
   case Shape::NONE:
      std::snprintf(b, sizeof(b), "%s", d.name);
      break;
   case Shape::RD_RS1_RS2:
      std::snprintf(b, sizeof(b), "%s R%u, R%u, R%u", d.name, fRd(w), fRs1(w), fRs2(w));
      break;
   case Shape::RD_RS1:
   {
      uint32_t dest = (d.fmt == Fmt::I) ? fRs2(w) : fRd(w);
      std::snprintf(b, sizeof(b), "%s R%u, R%u", d.name, dest, fRs1(w));
      break;
   }
   case Shape::RD_RS1_RS2_RS3:
      std::snprintf(b, sizeof(b), "%s R%u, R%u, R%u, R%u", d.name, fRd(w), fRs1(w), fRs2(w),
                    fShamt(w));
      break;
   case Shape::RS1_RS2:
      std::snprintf(b, sizeof(b), "%s R%u, R%u", d.name, fRs1(w), fRs2(w));
      break;
   case Shape::RT_RS1_IMM:
      std::snprintf(b, sizeof(b), "%s R%u, R%u, %d", d.name, fRs2(w), fRs1(w), simm);
      break;
   case Shape::RT_IMM:
      std::snprintf(b, sizeof(b), "%s R%u, %d", d.name, fRs2(w), simm);
      break;
   case Shape::BR:
      std::snprintf(b, sizeof(b), "%s R%u, R%u, 0x%04X", d.name, fRs1(w), fRs2(w), fImm16(w));
      break;
   case Shape::TGT:
      std::snprintf(b, sizeof(b), "%s 0x%07X", d.name, fTarget26(w));
      break;
   case Shape::RS1:
      std::snprintf(b, sizeof(b), "%s R%u", d.name, fRs1(w));
      break;
   case Shape::RS1_RT:
      std::snprintf(b, sizeof(b), "%s R%u, R%u", d.name, fRs1(w), fRs2(w));
      break;
   }
   return b;
}
