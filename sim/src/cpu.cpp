/**
 * @file cpu.cpp
 * @brief Instruction decoder and execution loop.
 */
#include "cpu.h"

#include <bit>
#include <cmath>

namespace
{

/// Destination register for writes: R0 and R1 are read-only, so redirect them to the sink.
inline uint8_t destination(uint32_t reg)
{
   return reg < 2 ? Cpu::SINK : static_cast<uint8_t>(reg);
}

/// Split a raw instruction word into the fields its operand shape uses.
Decoded decode(uint32_t word)
{
   Decoded in;
   in.op = opFromOpcode(fOpcode(word));
   if (in.op == Op::ILLEGAL)
      return in;

   int32_t simm = static_cast<int16_t>(fImm16(word));
   switch (desc(in.op).shape)
   {
   case Shape::NONE:
      break;
   case Shape::RD_RS1_RS2:
      in.rd = destination(fRd(word));
      in.rs1 = fRs1(word);
      in.rs2 = fRs2(word);
      break;
   case Shape::RD_RS1:
      in.rd = destination(fRd(word));
      in.rs1 = fRs1(word);
      break;
   case Shape::RD_RS1_RS2_RS3:
      in.rd = destination(fRd(word));
      in.rs1 = fRs1(word);
      in.rs2 = fRs2(word);
      in.rs3 = fShamt(word);
      break;
   case Shape::RS1_RS2:
      in.rs1 = fRs1(word);
      in.rs2 = fRs2(word);
      break;
   case Shape::RT_RS1_IMM:
      in.rs1 = fRs1(word);
      in.imm = simm;
      if (in.op == Op::ST || in.op == Op::FSW)
         in.rs2 = fRs2(word); // rt is the value being stored
      else
         in.rd = destination(fRs2(word)); // rt is the destination
      break;
   case Shape::RT_RS1:
      in.rd = destination(fRs2(word));
      in.rs1 = fRs1(word);
      break;
   case Shape::RT_IMM:
      in.rd = destination(fRs2(word));
      in.imm = simm;
      break;
   case Shape::BR:
      in.rs1 = fRs1(word);
      in.rs2 = fRs2(word);
      in.imm = static_cast<int32_t>(fImm16(word));
      break;
   case Shape::TGT:
      in.imm = static_cast<int32_t>(fTarget26(word));
      break;
   case Shape::RS1:
      in.rs1 = fRs1(word);
      break;
   case Shape::RS1_RT:
      in.rs1 = fRs1(word);
      in.rd = destination(fRs2(word));
      break;
   }
   return in;
}

/// Reinterpret register bits as an IEEE-754 binary32 value.
inline float asFloat(uint32_t bits)
{
   return std::bit_cast<float>(bits);
}

/// Reinterpret a binary32 value as register bits.
inline uint32_t asBits(float value)
{
   return std::bit_cast<uint32_t>(value);
}

/// Reinterpret register bits as a signed integer.
inline int32_t asSigned(uint32_t bits)
{
   return static_cast<int32_t>(bits);
}

/// Float to signed word: truncates toward zero, saturates on overflow, NaN becomes 0.
inline uint32_t floatToWord(float value)
{
   if (std::isnan(value))
      return 0;
   if (value >= 2147483648.0f)
      return 0x7FFFFFFFu;
   if (value <= -2147483648.0f)
      return 0x80000000u;
   return static_cast<uint32_t>(static_cast<int32_t>(value));
}

} // namespace

void Cpu::load(const Program &p)
{
   progSize = p.size();
   prog.assign(progSize + 1, Decoded{});
   for (uint32_t i = 0; i < progSize; ++i)
   {
      if (p.filled[i])
         prog[i] = decode(p.words[i]);
   }
}

Stop Cpu::run(uint64_t budget)
{
   // Work on local copies of the state the loop touches every iteration.
   uint32_t *const reg = R;
   const Decoded *const program = prog.data();
   uint32_t currentPc = pc;
   uint32_t flags = fr;

   // Save the local state back into the Cpu and report why we stopped.
   auto stop = [&](Stop reason)
   {
      pc = currentPc;
      fr = flags;
      return reason;
   };
   auto raiseFault = [&](const char *message, uint32_t address)
   {
      fault = message;
      faultAddr = address;
      return stop(Stop::Fault);
   };
   // Write an ALU result and update the n and z flags from it.
   auto aluWrite = [&](uint8_t rd, uint32_t result)
   {
      reg[rd] = result;
      uint32_t negative = (result >> 28) & 8u;
      uint32_t zero = result == 0 ? 4u : 0u;
      flags = (flags & 3u) | negative | zero;
   };

   while (budget)
   {
      --budget;
      const Decoded &in = program[currentPc];
      ++count[static_cast<int>(in.op)];
      uint32_t nextPc = currentPc + 1;

      switch (in.op)
      {
      // Other
      case Op::CLZ:
         reg[in.rd] = static_cast<uint32_t>(std::countl_zero(reg[in.rs1]));
         break;
      case Op::CNTT0:
         reg[in.rd] = static_cast<uint32_t>(std::countr_zero(reg[in.rs1]));
         break;
      case Op::POP:
         reg[in.rd] = static_cast<uint32_t>(std::popcount(reg[in.rs1]));
         break;
      case Op::BYTR:
         reg[in.rd] = __builtin_bswap32(reg[in.rs1]);
         break;
      case Op::SEXTD:
         reg[in.rd] = static_cast<uint32_t>(static_cast<int16_t>(reg[in.rs1] & 0xFFFF));
         break;
      case Op::NOP:
         break;

      // Memory
      case Op::LD:
      case Op::FLW:
      {
         uint32_t address = reg[in.rs1] + static_cast<uint32_t>(in.imm);
         if (address > Memory::MAX_ADDR)
            return raiseFault("data address out of range", address);
         reg[in.rd] = mem.read(address);
         break;
      }
      case Op::ST:
      case Op::FSW:
      {
         uint32_t address = reg[in.rs1] + static_cast<uint32_t>(in.imm);
         if (address > Memory::MAX_ADDR)
            return raiseFault("data address out of range", address);
         mem.write(address, reg[in.rs2]);
         break;
      }
      case Op::MOV:
         reg[in.rd] = reg[in.rs1];
         break;
      case Op::LDR:
         reg[in.rd] = static_cast<uint32_t>(in.imm);
         break;

      // ALU
      case Op::ADD:
         aluWrite(in.rd, reg[in.rs1] + reg[in.rs2]);
         break;
      case Op::SUB:
         aluWrite(in.rd, reg[in.rs1] - reg[in.rs2]);
         break;
      case Op::RSB:
         aluWrite(in.rd, reg[in.rs2] - reg[in.rs1]);
         break;
      case Op::INC:
         aluWrite(in.rd, reg[in.rs1] + 1);
         break;
      case Op::DEC:
         aluWrite(in.rd, reg[in.rs1] - 1);
         break;
      case Op::RS:
         aluWrite(in.rd, reg[in.rs1] >> 1);
         break;
      case Op::LS:
         aluWrite(in.rd, reg[in.rs1] << 1);
         break;
      case Op::AND:
         aluWrite(in.rd, reg[in.rs1] & reg[in.rs2]);
         break;
      case Op::OR:
         aluWrite(in.rd, reg[in.rs1] | reg[in.rs2]);
         break;
      case Op::NAND:
         aluWrite(in.rd, ~(reg[in.rs1] & reg[in.rs2]));
         break;
      case Op::NOR:
         aluWrite(in.rd, ~(reg[in.rs1] | reg[in.rs2]));
         break;
      case Op::XOR:
         aluWrite(in.rd, reg[in.rs1] ^ reg[in.rs2]);
         break;
      case Op::NOT:
         aluWrite(in.rd, ~reg[in.rs1]);
         break;
      case Op::ADDI:
         aluWrite(in.rd, reg[in.rs1] + static_cast<uint32_t>(in.imm));
         break;
      case Op::SUBI:
         aluWrite(in.rd, reg[in.rs1] - static_cast<uint32_t>(in.imm));
         break;
      case Op::SFLIP:
         aluWrite(in.rd, 0u - reg[in.rs1]);
         break;

      // Floating point (shares the integer register file)
      case Op::FADD:
         reg[in.rd] = asBits(asFloat(reg[in.rs1]) + asFloat(reg[in.rs2]));
         break;
      case Op::FSUB:
         reg[in.rd] = asBits(asFloat(reg[in.rs1]) - asFloat(reg[in.rs2]));
         break;
      case Op::FMULT:
         reg[in.rd] = asBits(asFloat(reg[in.rs1]) * asFloat(reg[in.rs2]));
         break;
      case Op::FDIV:
         reg[in.rd] = asBits(asFloat(reg[in.rs1]) / asFloat(reg[in.rs2]));
         break;
      case Op::FFMA:
         reg[in.rd] =
             asBits(std::fmaf(asFloat(reg[in.rs1]), asFloat(reg[in.rs2]), asFloat(reg[in.rs3])));
         break;
      case Op::FSQRT:
         reg[in.rd] = asBits(std::sqrt(asFloat(reg[in.rs1])));
         break;
      case Op::FNEG:
         reg[in.rd] = reg[in.rs1] ^ 0x80000000u;
         break;
      case Op::FABS:
         reg[in.rd] = reg[in.rs1] & 0x7FFFFFFFu;
         break;
      case Op::FLT:
         flags = (flags & ~2u) | (asFloat(reg[in.rs1]) < asFloat(reg[in.rs2]) ? 2u : 0u);
         break;
      case Op::FEQ:
         flags = (flags & ~1u) | (asFloat(reg[in.rs1]) == asFloat(reg[in.rs2]) ? 1u : 0u);
         break;
      case Op::FMAX:
      {
         bool firstIsLarger = asFloat(reg[in.rs1]) > asFloat(reg[in.rs2]);
         reg[in.rd] = firstIsLarger ? reg[in.rs1] : reg[in.rs2];
         break;
      }
      case Op::FTOIW:
         reg[in.rd] = floatToWord(asFloat(reg[in.rs1]));
         break;
      case Op::IWTOF:
         reg[in.rd] = asBits(static_cast<float>(asSigned(reg[in.rs1])));
         break;
      case Op::FMOV:
         reg[in.rd] = reg[in.rs1];
         break;

      // Control
      case Op::BEQ:
         if (reg[in.rs1] == reg[in.rs2])
            nextPc = static_cast<uint32_t>(in.imm);
         break;
      case Op::BNE:
         if (reg[in.rs1] != reg[in.rs2])
            nextPc = static_cast<uint32_t>(in.imm);
         break;
      case Op::BLT:
         if (asSigned(reg[in.rs1]) < asSigned(reg[in.rs2]))
            nextPc = static_cast<uint32_t>(in.imm);
         break;
      case Op::BLE:
         if (asSigned(reg[in.rs1]) <= asSigned(reg[in.rs2]))
            nextPc = static_cast<uint32_t>(in.imm);
         break;
      case Op::BGT:
         if (asSigned(reg[in.rs1]) > asSigned(reg[in.rs2]))
            nextPc = static_cast<uint32_t>(in.imm);
         break;
      case Op::BGE:
         if (asSigned(reg[in.rs1]) >= asSigned(reg[in.rs2]))
            nextPc = static_cast<uint32_t>(in.imm);
         break;
      case Op::JUMP:
         sr = nextPc;
         nextPc = static_cast<uint32_t>(in.imm);
         break;
      case Op::JUMPR:
      {
         uint32_t target = reg[in.rs1];
         if (target >= progSize)
            return raiseFault("jump target outside the program", target);
         sr = nextPc;
         nextPc = target;
         break;
      }
      case Op::JLINK:
      {
         uint32_t target = reg[in.rs1];
         if (target >= progSize)
            return raiseFault("jump target outside the program", target);
         reg[in.rd] = nextPc;
         sr = nextPc;
         nextPc = target;
         break;
      }
      case Op::HALT:
         return stop(Stop::Halted);

      case Op::ILLEGAL:
         return raiseFault("illegal instruction / PC outside program", currentPc);
      }

      currentPc = nextPc;
   }
   return stop(Stop::Budget);
}
