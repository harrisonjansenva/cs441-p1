#include "isa.h"

#include <cstdio>
#include <string>

namespace Simulator
{
#define SIM_OP_DESC(name, opcode, fmt, shape, type)                                                \
   {#name, opcode, Fmt::fmt, Shape::shape, Instr_type::type},
const InstrDesc ISA[NUM_OPS] = {SIM_OP_LIST(SIM_OP_DESC)};
#undef SIM_OP_DESC

const ShapeLayout &layout(Shape shape)
{
   using O = Operand;

   // indexed by Shape, so keep the same order as the enum
   static const ShapeLayout layouts[] = {
       {0, {}},                              // NONE
       {3, {O::Rd, O::Rs1, O::Rs2}},         // RD_RS1_RS2
       {2, {O::Rd, O::Rs1}},                 // RD_RS1
       {4, {O::Rd, O::Rs1, O::Rs2, O::Rs3}}, // RD_RS1_RS2_RS3
       {2, {O::Rs1, O::Rs2}},                // RS1_RS2
       {3, {O::Rt, O::Rs1, O::Imm16}},       // RT_RS1_IMM
       {2, {O::Rt, O::Imm16}},               // RT_IMM
       {2, {O::Rt, O::Rs1}},                 // RT_RS1
       {3, {O::Rs1, O::Rs2, O::Target16}},   // BR
       {1, {O::Target26}},                   // TGT
       {1, {O::Rs1}},                        // RS1
       {2, {O::Rs1, O::Rt}},                 // RS1_RT
   };
   return layouts[static_cast<std::size_t>(shape)];
}

unsigned operandShift(Operand operand)
{
   switch (operand)
   {
   case Operand::Rs1:
      return 21;
   case Operand::Rs2:
   case Operand::Rt:
      return 16;
   case Operand::Rd:
      return 11;
   case Operand::Rs3:
      return 6;
   case Operand::Imm16:
   case Operand::Target16:
   case Operand::Target26:
      return 0;
   }
   return 0;
}

bool isRegister(Operand operand)
{
   switch (operand)
   {
   case Operand::Rd:
   case Operand::Rs1:
   case Operand::Rs2:
   case Operand::Rt:
   case Operand::Rs3:
      return true;
   case Operand::Imm16:
   case Operand::Target16:
   case Operand::Target26:
      return false;
   }
   return false;
}

const InstrDesc &desc(Op op)
{
   static const InstrDesc illegalDesc{"ILLEGAL", 0xFF, Fmt::N, Shape::NONE, Instr_type::Other};
   std::size_t idx{static_cast<std::size_t>(op)};

   if (NUM_OPS > idx)
   {
      return ISA[idx];
   }
   return illegalDesc;
}

const char *typeName(Instr_type type)
{
   switch (type)
   {
   case Instr_type::Other:
      return "Other";
   case Instr_type::Memory:
      return "Memory";
   case Instr_type::ALU:
      return "ALU";
   case Instr_type::FP:
      return "Floating Point";
   case Instr_type::Control:
      return "Control";
   }
   return "?";
}

Op fromOpcode(std::uint32_t opcode)
{
   for (std::size_t i{0}; i < NUM_OPS; i++)
   {
      if (opcode == ISA[i].opcode)
      {
         return static_cast<Op>(i);
      }
   }
   return Op::ILLEGAL;
}

Op fromMnemonic(const std::string &upper)
{
   for (std::size_t i{0}; i < NUM_OPS; i++)
   {
      if (upper == ISA[i].name)
      {
         return static_cast<Op>(i);
      }
   }
   return Op::ILLEGAL;
}

std::string disassemble(std::uint32_t word)
{
   Op op{fromOpcode(fOpcode(word))};
   char buffer[48];

   if (Op::ILLEGAL == op)
   {
      std::snprintf(buffer, sizeof(buffer), ".word 0x%08X", static_cast<unsigned>(word));
      return buffer;
   }

   const InstrDesc &d{desc(op)};
   const ShapeLayout &shape{layout(d.shape)};
   std::string text{d.name};

   for (std::size_t i{0}; i < shape.count; i++)
   {
      Operand operand{shape.operands[i]};
      unsigned field{(word >> operandShift(operand)) & 0x1F};

      switch (operand)
      {
      case Operand::Rd:
      case Operand::Rs1:
      case Operand::Rs2:
      case Operand::Rt:
      case Operand::Rs3:
         std::snprintf(buffer, sizeof(buffer), "R%u", field);
         break;
      case Operand::Imm16:
         std::snprintf(buffer, sizeof(buffer), "%d", static_cast<std::int16_t>(fImm16(word)));
         break;
      case Operand::Target16:
         std::snprintf(buffer, sizeof(buffer), "0x%04X", static_cast<unsigned>(fImm16(word)));
         break;
      case Operand::Target26:
         std::snprintf(buffer, sizeof(buffer), "0x%07X", static_cast<unsigned>(fTarget26(word)));
         break;
      }

      text += (0 == i) ? " " : ", ";
      text += buffer;
   }
   return text;
}

} // namespace Simulator
