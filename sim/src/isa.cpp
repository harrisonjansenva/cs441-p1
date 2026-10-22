/**
 * @file isa.cpp
 * @brief Opcode and mnemonic lookup, and the disassembler.
 */
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
   static const std::array<Op, 64> byOpcode = []
   {
      std::array<Op, 64> table;
      table.fill(Op::ILLEGAL);
      for (int i = 0; i < NUM_OPS; ++i)
         table[ISA[i].opcode] = static_cast<Op>(i);
      return table;
   }();
   return opcode6 < 64 ? byOpcode[opcode6] : Op::ILLEGAL;
}

Op opFromMnemonic(const std::string &upper)
{
   static const std::unordered_map<std::string, Op> byName = []
   {
      std::unordered_map<std::string, Op> table;
      for (int i = 0; i < NUM_OPS; ++i)
         table[ISA[i].name] = static_cast<Op>(i);
      table["CNTL0"] = Op::CLZ; // name used in earlier spec drafts
      table["NOOP"] = Op::NOP;
      return table;
   }();
   auto found = byName.find(upper);
   return found == byName.end() ? Op::ILLEGAL : found->second;
}

std::string disassemble(uint32_t word)
{
   Op op = opFromOpcode(fOpcode(word));
   if (op == Op::ILLEGAL)
   {
      char text[48];
      std::snprintf(text, sizeof text, ".word 0x%08X", word);
      return text;
   }
   const InstrDesc &info = desc(op);
   char text[96];
   int16_t simm = static_cast<int16_t>(fImm16(word));
   switch (info.shape)
   {
   case Shape::NONE:
      std::snprintf(text, sizeof text, "%s", info.name);
      break;
   case Shape::RD_RS1_RS2:
      std::snprintf(text, sizeof text, "%s R%u, R%u, R%u", info.name, fRd(word), fRs1(word),
                    fRs2(word));
      break;
   case Shape::RD_RS1:
      std::snprintf(text, sizeof text, "%s R%u, R%u", info.name, fRd(word), fRs1(word));
      break;
   case Shape::RD_RS1_RS2_RS3:
      std::snprintf(text, sizeof text, "%s R%u, R%u, R%u, R%u", info.name, fRd(word), fRs1(word),
                    fRs2(word), fShamt(word));
      break;
   case Shape::RS1_RS2:
      std::snprintf(text, sizeof text, "%s R%u, R%u", info.name, fRs1(word), fRs2(word));
      break;
   case Shape::RT_RS1_IMM:
      std::snprintf(text, sizeof text, "%s R%u, R%u, %d", info.name, fRs2(word), fRs1(word), simm);
      break;
   case Shape::RT_RS1:
      std::snprintf(text, sizeof text, "%s R%u, R%u", info.name, fRs2(word), fRs1(word));
      break;
   case Shape::RT_IMM:
      std::snprintf(text, sizeof text, "%s R%u, %d", info.name, fRs2(word), simm);
      break;
   case Shape::BR:
      std::snprintf(text, sizeof text, "%s R%u, R%u, 0x%04X", info.name, fRs1(word), fRs2(word),
                    fImm16(word));
      break;
   case Shape::TGT:
      std::snprintf(text, sizeof text, "%s 0x%07X", info.name, fTarget26(word));
      break;
   case Shape::RS1:
      std::snprintf(text, sizeof text, "%s R%u", info.name, fRs1(word));
      break;
   case Shape::RS1_RT:
      std::snprintf(text, sizeof text, "%s R%u, R%u", info.name, fRs1(word), fRs2(word));
      break;
   }
   return text;
}
