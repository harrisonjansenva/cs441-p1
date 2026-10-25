#include "cpu.h"

#include <cmath>
#include <cstring>

namespace Simulator
{
namespace
{
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Helper to convert raw std::uint32_t to float
////////////////////////////////////////////////////////////////////////////////////////////////////
float uintToFloat(std::uint32_t u)
{
   float f;
   std::memcpy(&f, &u, sizeof(float));
   return f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Helper to convert float to raw std::uint32_t
////////////////////////////////////////////////////////////////////////////////////////////////////
std::uint32_t floatToUint(float f)
{
   std::uint32_t u;
   std::memcpy(&u, &f, sizeof(std::uint32_t));
   return u;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Convert float to signed 32-bit integer word with saturation
////////////////////////////////////////////////////////////////////////////////////////////////////
std::uint32_t floatToIntWord(float f)
{
   if (std::isnan(f))
   {
      return 0;
   }
   if (f >= 2147483648.0f)
   {
      return 0x7FFFFFFFu;
   }
   if (f <= -2147483648.0f)
   {
      return 0x80000000u;
   }
   return static_cast<std::uint32_t>(static_cast<std::int32_t>(f));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Count leading zeros
////////////////////////////////////////////////////////////////////////////////////////////////////
std::uint32_t countLeadingZeros(std::uint32_t val)
{
   if (0 == val)
   {
      return 32;
   }

   std::uint32_t count{0};
   for (int i{31}; i >= 0; i--)
   {
      if ((val >> i) & 1)
      {
         break;
      }
      count++;
   }
   return count;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Count trailing zeros
////////////////////////////////////////////////////////////////////////////////////////////////////
std::uint32_t countTrailingZeros(std::uint32_t val)
{
   if (0 == val)
   {
      return 32;
   }

   std::uint32_t count{0};
   while (0 == (val & 1))
   {
      count++;
      val >>= 1;
   }
   return count;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Population count (number of set bits)
////////////////////////////////////////////////////////////////////////////////////////////////////
std::uint32_t popCount(std::uint32_t val)
{
   std::uint32_t count{0};
   for (int i{0}; i < 32; i++)
   {
      if ((val >> i) & 1)
      {
         count++;
      }
   }
   return count;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Reverse byte order
////////////////////////////////////////////////////////////////////////////////////////////////////
std::uint32_t reverseBytes(std::uint32_t val)
{
   return ((val & 0x000000FF) << 24) | ((val & 0x0000FF00) << 8) | ((val & 0x00FF0000) >> 8) |
          ((val & 0xFF000000) >> 24);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Update negative (bit 3) and zero (bit 2) flags based on ALU result
////////////////////////////////////////////////////////////////////////////////////////////////////
void updateALUFlags(std::uint32_t &fr, std::uint32_t result)
{
   std::uint32_t n{(result >> 31) & 1u};
   std::uint32_t z{(0 == result) ? 1u : 0u};
   fr = (fr & 0x3) | (n << 3) | (z << 2);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief decide if a conditional branch is taken, comparing the registers as signed numbers.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool branchTaken(Op op, std::int32_t a, std::int32_t b)
{
   switch (op)
   {
   case Op::BEQ:
      return a == b;
   case Op::BLT:
      return a < b;
   case Op::BLE:
      return a <= b;
   case Op::BGT:
      return a > b;
   case Op::BGE:
      return a >= b;
   case Op::BNE:
      return a != b;
   default:
      return false;
   }
}

} // anonymous namespace

Stop run(Cpu &c, std::uint64_t budget)
{
   auto raise = [&c](const char *why, std::uint32_t addr)
   {
      c.fault = why;
      c.faultAddr = addr;
      return Stop::Fault;
   };

   while (budget > 0)
   {
      if (!c.program.isFilled(c.pc))
      {
         return raise("illegal instruction", c.pc);
      }

      std::uint32_t word{c.program.entries[c.pc].word};
      Op op{fromOpcode(fOpcode(word))};
      if (Op::ILLEGAL == op)
      {
         return raise("illegal instruction", c.pc);
      }

      budget--;
      c.count[static_cast<std::size_t>(op)]++;

      // Decode instruction fields
      std::uint32_t rs1{fRs1(word)};
      std::uint32_t rs2{fRs2(word)}; // also rt
      std::uint32_t rd{fRd(word)};
      std::uint32_t shamt{fShamt(word)};
      std::int32_t simm{static_cast<std::int16_t>(fImm16(word))};

      // everything but a taken branch or a jump continues with the next instruction
      std::uint32_t nextPc{c.pc + 1};

      // write an ALU result, which also updates the flags
      auto alu = [&c](std::uint32_t dest, std::uint32_t res)
      {
         c.setReg(dest, res);
         updateALUFlags(c.fr, res);
      };
      // write a floating point result
      auto fp = [&c](std::uint32_t dest, float f) { c.setReg(dest, floatToUint(f)); };
      // check that a jump or branch lands inside the program
      auto badTarget = [&c](std::uint32_t target) { return target >= c.program.size(); };

      switch (op)
      {
      // --- Other ---
      case Op::CLZ:
         c.setReg(rd, countLeadingZeros(c.R[rs1]));
         break;
      case Op::CNTT0:
         c.setReg(rd, countTrailingZeros(c.R[rs1]));
         break;
      case Op::POP:
         c.setReg(rd, popCount(c.R[rs1]));
         break;
      case Op::BYTR:
         c.setReg(rd, reverseBytes(c.R[rs1]));
         break;
      case Op::SEXTD:
      {
         std::int16_t low16{static_cast<std::int16_t>(c.R[rs1] & 0xFFFF)};
         c.setReg(rd, static_cast<std::uint32_t>(static_cast<std::int32_t>(low16)));
         break;
      }
      case Op::NOP:
         break;

      // --- Memory ---
      case Op::LD:
      case Op::FLW:
      {
         std::uint32_t ea{c.R[rs1] + static_cast<std::uint32_t>(simm)};
         if (ea > MAX_ADDR)
         {
            return raise("data address out of range", ea);
         }
         c.setReg(rs2, c.mem.read(ea)); // rt is rs2
         break;
      }
      case Op::ST:
      case Op::FSW:
      {
         std::uint32_t ea{c.R[rs1] + static_cast<std::uint32_t>(simm)};
         if (ea > MAX_ADDR)
         {
            return raise("data address out of range", ea);
         }
         c.mem.write(ea, c.R[rs2]); // rt is stored
         break;
      }
      case Op::MOV:
         c.setReg(rs2, c.R[rs1]);
         break;
      case Op::LDR:
         c.setReg(rs2, static_cast<std::uint32_t>(simm));
         break;

      // --- ALU ---
      case Op::ADD:
         alu(rd, c.R[rs1] + c.R[rs2]);
         break;
      case Op::SUB:
         alu(rd, c.R[rs1] - c.R[rs2]);
         break;
      case Op::RSB:
         alu(rd, c.R[rs2] - c.R[rs1]);
         break;
      case Op::INC:
         alu(rd, c.R[rs1] + 1);
         break;
      case Op::DEC:
         alu(rd, c.R[rs1] - 1);
         break;
      case Op::RS:
         alu(rd, c.R[rs1] >> 1);
         break;
      case Op::LS:
         alu(rd, c.R[rs1] << 1);
         break;
      case Op::AND:
         alu(rd, c.R[rs1] & c.R[rs2]);
         break;
      case Op::OR:
         alu(rd, c.R[rs1] | c.R[rs2]);
         break;
      case Op::NAND:
         alu(rd, ~(c.R[rs1] & c.R[rs2]));
         break;
      case Op::NOR:
         alu(rd, ~(c.R[rs1] | c.R[rs2]));
         break;
      case Op::XOR:
         alu(rd, c.R[rs1] ^ c.R[rs2]);
         break;
      case Op::NOT:
         alu(rd, ~c.R[rs1]);
         break;
      case Op::ADDI:
         alu(rs2, c.R[rs1] + static_cast<std::uint32_t>(simm));
         break;
      case Op::SUBI:
         alu(rs2, c.R[rs1] - static_cast<std::uint32_t>(simm));
         break;
      case Op::SFLIP:
         alu(rd, static_cast<std::uint32_t>(-static_cast<std::int32_t>(c.R[rs1])));
         break;

      // --- Floating Point ---
      case Op::FADD:
         fp(rd, uintToFloat(c.R[rs1]) + uintToFloat(c.R[rs2]));
         break;
      case Op::FSUB:
         fp(rd, uintToFloat(c.R[rs1]) - uintToFloat(c.R[rs2]));
         break;
      case Op::FMULT:
         fp(rd, uintToFloat(c.R[rs1]) * uintToFloat(c.R[rs2]));
         break;
      case Op::FDIV:
         fp(rd, uintToFloat(c.R[rs1]) / uintToFloat(c.R[rs2]));
         break;
      case Op::FFMA:
         fp(rd, std::fma(uintToFloat(c.R[rs1]), uintToFloat(c.R[rs2]), uintToFloat(c.R[shamt])));
         break;
      case Op::FSQRT:
         fp(rd, std::sqrt(uintToFloat(c.R[rs1])));
         break;
      case Op::FNEG:
         c.setReg(rd, c.R[rs1] ^ 0x80000000u);
         break;
      case Op::FABS:
         c.setReg(rd, c.R[rs1] & 0x7FFFFFFFu);
         break;
      case Op::FLT:
      {
         bool less{uintToFloat(c.R[rs1]) < uintToFloat(c.R[rs2])};
         c.fr = (c.fr & ~2u) | (less ? 2u : 0u);
         break;
      }
      case Op::FEQ:
      {
         bool equal{uintToFloat(c.R[rs1]) == uintToFloat(c.R[rs2])};
         c.fr = (c.fr & ~1u) | (equal ? 1u : 0u);
         break;
      }
      case Op::FMAX:
      {
         float a{uintToFloat(c.R[rs1])};
         float b{uintToFloat(c.R[rs2])};
         fp(rd, (a > b) ? a : b);
         break;
      }
      case Op::FTOIW:
         c.setReg(rd, floatToIntWord(uintToFloat(c.R[rs1])));
         break;
      case Op::IWTOF:
         fp(rd, static_cast<float>(static_cast<std::int32_t>(c.R[rs1])));
         break;
      case Op::FMOV:
         c.setReg(rd, c.R[rs1]);
         break;

      // --- Control ---
      case Op::BEQ:
      case Op::BLT:
      case Op::BLE:
      case Op::BGT:
      case Op::BGE:
      case Op::BNE:
         if (branchTaken(op, static_cast<std::int32_t>(c.R[rs1]),
                         static_cast<std::int32_t>(c.R[rs2])))
         {
            std::uint32_t target{fImm16(word)};
            if (badTarget(target))
            {
               return raise("jump target outside the program", target);
            }
            nextPc = target;
         }
         break;
      case Op::JUMP:
      case Op::JUMPR:
      case Op::JLINK:
      {
         std::uint32_t target{(Op::JUMP == op) ? fTarget26(word) : c.R[rs1]};
         if (badTarget(target))
         {
            return raise("jump target outside the program", target);
         }
         if (Op::JLINK == op)
         {
            c.setReg(rs2, c.pc + 1); // Save link in rt (rs2)
         }
         c.sr = c.pc + 1;
         nextPc = target;
         break;
      }
      case Op::HALT:
         return Stop::Halted;

      case Op::ILLEGAL:
         return raise("illegal instruction", c.pc);
      }

      c.pc = nextPc;
   }

   return Stop::Budget;
}

} // namespace Simulator
