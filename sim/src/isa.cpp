#include "isa.h"

#include <cstdio>
#include <unordered_map>

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
   static const std::array<Op, 64> table = []
   {
      std::array<Op, 64> t;
      t.fill(Op::ILLEGAL);
      for (int i = 0; i < NUM_OPS; ++i)
         t[ISA[i].opcode] = static_cast<Op>(i);
      return t;
   }();
   return opcode6 < 64 ? table[opcode6] : Op::ILLEGAL;
}

Op opFromMnemonic(const std::string &upper)
{
   static const std::unordered_map<std::string, Op> table = []
   {
      std::unordered_map<std::string, Op> t;
      for (int i = 0; i < NUM_OPS; ++i)
         t[ISA[i].name] = static_cast<Op>(i);
      t["CNTL0"] = Op::CLZ; // earlier spec drafts
      t["NOOP"] = Op::NOP;
      return t;
   }();
   auto it = table.find(upper);
   return it == table.end() ? Op::ILLEGAL : it->second;
}

std::string disassemble(uint32_t w)
{
   Op op = opFromOpcode(fOpcode(w));
   if (op == Op::ILLEGAL)
   {
      char b[48];
      std::snprintf(b, sizeof b, ".word 0x%08X", w);
      return b;
   }
   const InstrDesc &d = desc(op);
   char b[96];
   int16_t simm = static_cast<int16_t>(fImm16(w));
   switch (d.shape)
   {
   case Shape::NONE:
      std::snprintf(b, sizeof b, "%s", d.name);
      break;
   case Shape::RD_RS1_RS2:
      std::snprintf(b, sizeof b, "%s R%u, R%u, R%u", d.name, fRd(w), fRs1(w), fRs2(w));
      break;
   case Shape::RD_RS1:
      std::snprintf(b, sizeof b, "%s R%u, R%u", d.name, fRd(w), fRs1(w));
      break;
   case Shape::RD_RS1_RS2_RS3:
      std::snprintf(b, sizeof b, "%s R%u, R%u, R%u, R%u", d.name, fRd(w), fRs1(w), fRs2(w),
                    fShamt(w));
      break;
   case Shape::RS1_RS2:
      std::snprintf(b, sizeof b, "%s R%u, R%u", d.name, fRs1(w), fRs2(w));
      break;
   case Shape::RT_RS1_IMM:
      std::snprintf(b, sizeof b, "%s R%u, R%u, %d", d.name, fRs2(w), fRs1(w), simm);
      break;
   case Shape::RT_RS1:
      std::snprintf(b, sizeof b, "%s R%u, R%u", d.name, fRs2(w), fRs1(w));
      break;
   case Shape::RT_IMM:
      std::snprintf(b, sizeof b, "%s R%u, %d", d.name, fRs2(w), simm);
      break;
   case Shape::BR:
      std::snprintf(b, sizeof b, "%s R%u, R%u, 0x%04X", d.name, fRs1(w), fRs2(w), fImm16(w));
      break;
   case Shape::TGT:
      std::snprintf(b, sizeof b, "%s 0x%07X", d.name, fTarget26(w));
      break;
   case Shape::RS1:
      std::snprintf(b, sizeof b, "%s R%u", d.name, fRs1(w));
      break;
   case Shape::RS1_RT:
      std::snprintf(b, sizeof b, "%s R%u, R%u", d.name, fRs1(w), fRs2(w));
      break;
   }
   return b;
}
