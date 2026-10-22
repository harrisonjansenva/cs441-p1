#include "cpu.h"

#include <cmath>
#include <cstring>

namespace
{

// Helper to convert raw uint32_t to float
float uintToFloat(uint32_t u)
{
   float f;
   std::memcpy(&f, &u, sizeof(float));
   return f;
}

// Helper to convert float to raw uint32_t
uint32_t floatToUint(float f)
{
   uint32_t u;
   std::memcpy(&u, &f, sizeof(uint32_t));
   return u;
}

// Convert float to signed 32-bit integer word with saturation
uint32_t floatToIntWord(float f)
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
   return static_cast<uint32_t>(static_cast<int32_t>(f));
}

// Count leading zeros
uint32_t countLeadingZeros(uint32_t val)
{
   if (val == 0)
      return 32;
   uint32_t count = 0;
   for (int i = 31; i >= 0; i--)
   {
      if ((val >> i) & 1)
         break;
      count++;
   }
   return count;
}

// Count trailing zeros
uint32_t countTrailingZeros(uint32_t val)
{
   if (val == 0)
      return 32;
   uint32_t count = 0;
   while ((val & 1) == 0)
   {
      count++;
      val >>= 1;
   }
   return count;
}

// Population count (number of set bits)
uint32_t popCount(uint32_t val)
{
   uint32_t count = 0;
   for (int i = 0; i < 32; i++)
   {
      if ((val >> i) & 1)
      {
         count++;
      }
   }
   return count;
}

// Reverse byte order
uint32_t reverseBytes(uint32_t val)
{
   return ((val & 0x000000FF) << 24) | ((val & 0x0000FF00) << 8) | ((val & 0x00FF0000) >> 8) |
          ((val & 0xFF000000) >> 24);
}

// Update negative (bit 3) and zero (bit 2) flags based on ALU result
void updateALUFlags(uint32_t &fr, uint32_t result)
{
   uint32_t n = (result >> 31) & 1;
   uint32_t z = (result == 0) ? 1 : 0;
   fr = (fr & 0x3) | (n << 3) | (z << 2);
}

} // namespace

void Cpu::load(const Program &p)
{
   progSize = p.size();
   progWords = p.words;
   progFilled = p.filled;
}

Stop run(Cpu &c, uint64_t budget)
{
   while (budget > 0)
   {
      if (c.pc >= c.progSize || !c.progFilled[c.pc])
      {
         c.fault = "illegal instruction";
         c.faultAddr = c.pc;
         return Stop::Fault;
      }

      uint32_t word = c.progWords[c.pc];
      uint32_t opcode = fOpcode(word);
      Op op = opFromOpcode(opcode);
      if (op == Op::ILLEGAL)
      {
         c.fault = "illegal instruction";
         c.faultAddr = c.pc;
         return Stop::Fault;
      }

      budget--;
      c.count[static_cast<int>(op)]++;

      // Decode instruction fields
      uint32_t rs1 = fRs1(word);
      uint32_t rs2 = fRs2(word); // also rt
      uint32_t rd = fRd(word);
      uint32_t shamt = fShamt(word);
      int32_t simm = static_cast<int16_t>(fImm16(word));
      uint32_t target26 = fTarget26(word);

      switch (op)
      {
      // --- Other ---
      case Op::CLZ:
         c.setReg(rd, countLeadingZeros(c.R[rs1]));
         c.pc++;
         break;
      case Op::CNTT0:
         c.setReg(rd, countTrailingZeros(c.R[rs1]));
         c.pc++;
         break;
      case Op::POP:
         c.setReg(rd, popCount(c.R[rs1]));
         c.pc++;
         break;
      case Op::BYTR:
         c.setReg(rd, reverseBytes(c.R[rs1]));
         c.pc++;
         break;
      case Op::SEXTD:
      {
         int16_t low16 = static_cast<int16_t>(c.R[rs1] & 0xFFFF);
         c.setReg(rd, static_cast<uint32_t>(static_cast<int32_t>(low16)));
         c.pc++;
         break;
      }
      case Op::NOP:
         c.pc++;
         break;

      // --- Memory ---
      case Op::LD:
      {
         uint32_t ea = c.R[rs1] + static_cast<uint32_t>(simm);
         if (ea > Memory::MAX_ADDR)
         {
            c.fault = "data address out of range";
            c.faultAddr = ea;
            return Stop::Fault;
         }
         c.setReg(rs2, c.mem.read(ea)); // rt is rs2
         c.pc++;
         break;
      }
      case Op::ST:
      {
         uint32_t ea = c.R[rs1] + static_cast<uint32_t>(simm);
         if (ea > Memory::MAX_ADDR)
         {
            c.fault = "data address out of range";
            c.faultAddr = ea;
            return Stop::Fault;
         }
         c.mem.write(ea, c.R[rs2]); // rt is stored
         c.pc++;
         break;
      }
      case Op::MOV:
         c.setReg(rs2, c.R[rs1]);
         c.pc++;
         break;
      case Op::LDR:
         c.setReg(rs2, static_cast<uint32_t>(simm));
         c.pc++;
         break;

      // --- ALU ---
      case Op::ADD:
      {
         uint32_t res = c.R[rs1] + c.R[rs2];
         c.setReg(rd, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }
      case Op::SUB:
      {
         uint32_t res = c.R[rs1] - c.R[rs2];
         c.setReg(rd, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }
      case Op::RSB:
      {
         uint32_t res = c.R[rs2] - c.R[rs1];
         c.setReg(rd, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }
      case Op::INC:
      {
         uint32_t res = c.R[rs1] + 1;
         c.setReg(rd, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }
      case Op::DEC:
      {
         uint32_t res = c.R[rs1] - 1;
         c.setReg(rd, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }
      case Op::RS:
      {
         uint32_t res = c.R[rs1] >> 1;
         c.setReg(rd, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }
      case Op::LS:
      {
         uint32_t res = c.R[rs1] << 1;
         c.setReg(rd, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }
      case Op::AND:
      {
         uint32_t res = c.R[rs1] & c.R[rs2];
         c.setReg(rd, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }
      case Op::OR:
      {
         uint32_t res = c.R[rs1] | c.R[rs2];
         c.setReg(rd, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }
      case Op::NAND:
      {
         uint32_t res = ~(c.R[rs1] & c.R[rs2]);
         c.setReg(rd, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }
      case Op::NOR:
      {
         uint32_t res = ~(c.R[rs1] | c.R[rs2]);
         c.setReg(rd, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }
      case Op::XOR:
      {
         uint32_t res = c.R[rs1] ^ c.R[rs2];
         c.setReg(rd, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }
      case Op::NOT:
      {
         uint32_t res = ~c.R[rs1];
         c.setReg(rd, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }
      case Op::ADDI:
      {
         uint32_t res = c.R[rs1] + static_cast<uint32_t>(simm);
         c.setReg(rs2, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }
      case Op::SUBI:
      {
         uint32_t res = c.R[rs1] - static_cast<uint32_t>(simm);
         c.setReg(rs2, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }
      case Op::SFLIP:
      {
         uint32_t res = static_cast<uint32_t>(-static_cast<int32_t>(c.R[rs1]));
         c.setReg(rd, res);
         updateALUFlags(c.fr, res);
         c.pc++;
         break;
      }

      // --- Floating Point ---
      case Op::FLW:
      {
         uint32_t ea = c.R[rs1] + static_cast<uint32_t>(simm);
         if (ea > Memory::MAX_ADDR)
         {
            c.fault = "data address out of range";
            c.faultAddr = ea;
            return Stop::Fault;
         }
         c.setReg(rs2, c.mem.read(ea));
         c.pc++;
         break;
      }
      case Op::FSW:
      {
         uint32_t ea = c.R[rs1] + static_cast<uint32_t>(simm);
         if (ea > Memory::MAX_ADDR)
         {
            c.fault = "data address out of range";
            c.faultAddr = ea;
            return Stop::Fault;
         }
         c.mem.write(ea, c.R[rs2]);
         c.pc++;
         break;
      }
      case Op::FADD:
      {
         float f = uintToFloat(c.R[rs1]) + uintToFloat(c.R[rs2]);
         c.setReg(rd, floatToUint(f));
         c.pc++;
         break;
      }
      case Op::FSUB:
      {
         float f = uintToFloat(c.R[rs1]) - uintToFloat(c.R[rs2]);
         c.setReg(rd, floatToUint(f));
         c.pc++;
         break;
      }
      case Op::FMULT:
      {
         float f = uintToFloat(c.R[rs1]) * uintToFloat(c.R[rs2]);
         c.setReg(rd, floatToUint(f));
         c.pc++;
         break;
      }
      case Op::FDIV:
      {
         float f = uintToFloat(c.R[rs1]) / uintToFloat(c.R[rs2]);
         c.setReg(rd, floatToUint(f));
         c.pc++;
         break;
      }
      case Op::FFMA:
      {
         float f = std::fma(uintToFloat(c.R[rs1]), uintToFloat(c.R[rs2]), uintToFloat(c.R[shamt]));
         c.setReg(rd, floatToUint(f));
         c.pc++;
         break;
      }
      case Op::FSQRT:
      {
         float f = std::sqrt(uintToFloat(c.R[rs1]));
         c.setReg(rd, floatToUint(f));
         c.pc++;
         break;
      }
      case Op::FNEG:
         c.setReg(rd, c.R[rs1] ^ 0x80000000u);
         c.pc++;
         break;
      case Op::FABS:
         c.setReg(rd, c.R[rs1] & 0x7FFFFFFFu);
         c.pc++;
         break;
      case Op::FLT:
      {
         float a = uintToFloat(c.R[rs1]);
         float b = uintToFloat(c.R[rs2]);
         c.fr = (c.fr & ~2u) | ((a < b) ? 2u : 0u);
         c.pc++;
         break;
      }
      case Op::FEQ:
      {
         float a = uintToFloat(c.R[rs1]);
         float b = uintToFloat(c.R[rs2]);
         c.fr = (c.fr & ~1u) | ((a == b) ? 1u : 0u);
         c.pc++;
         break;
      }
      case Op::FMAX:
      {
         float a = uintToFloat(c.R[rs1]);
         float b = uintToFloat(c.R[rs2]);
         float r = (a > b) ? a : b;
         c.setReg(rd, floatToUint(r));
         c.pc++;
         break;
      }
      case Op::FTOIW:
         c.setReg(rd, floatToIntWord(uintToFloat(c.R[rs1])));
         c.pc++;
         break;
      case Op::IWTOF:
      {
         float f = static_cast<float>(static_cast<int32_t>(c.R[rs1]));
         c.setReg(rd, floatToUint(f));
         c.pc++;
         break;
      }
      case Op::FMOV:
         c.setReg(rd, c.R[rs1]);
         c.pc++;
         break;

      // --- Control ---
      case Op::BEQ:
      {
         int32_t v1 = static_cast<int32_t>(c.R[rs1]);
         int32_t v2 = static_cast<int32_t>(c.R[rs2]);
         if (v1 == v2)
         {
            uint32_t target = fImm16(word);
            if (target >= c.progSize)
            {
               c.fault = "jump target outside the program";
               c.faultAddr = target;
               return Stop::Fault;
            }
            c.pc = target;
         }
         else
         {
            c.pc++;
         }
         break;
      }
      case Op::BLT:
      {
         int32_t v1 = static_cast<int32_t>(c.R[rs1]);
         int32_t v2 = static_cast<int32_t>(c.R[rs2]);
         if (v1 < v2)
         {
            uint32_t target = fImm16(word);
            if (target >= c.progSize)
            {
               c.fault = "jump target outside the program";
               c.faultAddr = target;
               return Stop::Fault;
            }
            c.pc = target;
         }
         else
         {
            c.pc++;
         }
         break;
      }
      case Op::BLE:
      {
         int32_t v1 = static_cast<int32_t>(c.R[rs1]);
         int32_t v2 = static_cast<int32_t>(c.R[rs2]);
         if (v1 <= v2)
         {
            uint32_t target = fImm16(word);
            if (target >= c.progSize)
            {
               c.fault = "jump target outside the program";
               c.faultAddr = target;
               return Stop::Fault;
            }
            c.pc = target;
         }
         else
         {
            c.pc++;
         }
         break;
      }
      case Op::BGT:
      {
         int32_t v1 = static_cast<int32_t>(c.R[rs1]);
         int32_t v2 = static_cast<int32_t>(c.R[rs2]);
         if (v1 > v2)
         {
            uint32_t target = fImm16(word);
            if (target >= c.progSize)
            {
               c.fault = "jump target outside the program";
               c.faultAddr = target;
               return Stop::Fault;
            }
            c.pc = target;
         }
         else
         {
            c.pc++;
         }
         break;
      }
      case Op::BGE:
      {
         int32_t v1 = static_cast<int32_t>(c.R[rs1]);
         int32_t v2 = static_cast<int32_t>(c.R[rs2]);
         if (v1 >= v2)
         {
            uint32_t target = fImm16(word);
            if (target >= c.progSize)
            {
               c.fault = "jump target outside the program";
               c.faultAddr = target;
               return Stop::Fault;
            }
            c.pc = target;
         }
         else
         {
            c.pc++;
         }
         break;
      }
      case Op::BNE:
      {
         int32_t v1 = static_cast<int32_t>(c.R[rs1]);
         int32_t v2 = static_cast<int32_t>(c.R[rs2]);
         if (v1 != v2)
         {
            uint32_t target = fImm16(word);
            if (target >= c.progSize)
            {
               c.fault = "jump target outside the program";
               c.faultAddr = target;
               return Stop::Fault;
            }
            c.pc = target;
         }
         else
         {
            c.pc++;
         }
         break;
      }
      case Op::JUMP:
      {
         uint32_t target = target26;
         if (target >= c.progSize)
         {
            c.fault = "jump target outside the program";
            c.faultAddr = target;
            return Stop::Fault;
         }
         c.sr = c.pc + 1;
         c.pc = target;
         break;
      }
      case Op::JUMPR:
      {
         uint32_t target = c.R[rs1];
         if (target >= c.progSize)
         {
            c.fault = "jump target outside the program";
            c.faultAddr = target;
            return Stop::Fault;
         }
         c.sr = c.pc + 1;
         c.pc = target;
         break;
      }
      case Op::JLINK:
      {
         uint32_t target = c.R[rs1];
         if (target >= c.progSize)
         {
            c.fault = "jump target outside the program";
            c.faultAddr = target;
            return Stop::Fault;
         }
         c.setReg(rs2, c.pc + 1); // Save link in rt (rs2)
         c.sr = c.pc + 1;
         c.pc = target;
         break;
      }
      case Op::HALT:
         return Stop::Halted;

      case Op::ILLEGAL:
         c.fault = "illegal instruction";
         c.faultAddr = c.pc;
         return Stop::Fault;
      }
   }

   return Stop::Budget;
}
