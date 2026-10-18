#include "cpu.h"

#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

namespace
{

inline uint8_t wr(uint32_t r)
{
   return r < 2 ? Cpu::SINK : static_cast<uint8_t>(r);
}

Decoded decode(uint32_t w)
{
   Decoded x;
   x.op = opFromOpcode(fOpcode(w));
   if (x.op == Op::ILLEGAL)
      return x;
   int32_t simm = static_cast<int16_t>(fImm16(w));
   switch (desc(x.op).shape)
   {
   case Shape::NONE:
      break;
   case Shape::RD_RS1_RS2:
      x.d = wr(fRd(w)), x.s1 = fRs1(w), x.s2 = fRs2(w);
      break;
   case Shape::RD_RS1:
      x.d = wr(fRd(w)), x.s1 = fRs1(w);
      break;
   case Shape::RD:
      x.d = wr(fRd(w)), x.s1 = fRd(w);
      break; // OUT reads s1
   case Shape::RD_RS1_RS2_RS3:
      x.d = wr(fRd(w)), x.s1 = fRs1(w), x.s2 = fRs2(w), x.s3 = fShamt(w);
      break;
   case Shape::RS1_RS2:
      x.s1 = fRs1(w), x.s2 = fRs2(w);
      break;
   case Shape::RT_RS1_IMM:
      x.s1 = fRs1(w), x.imm = simm;
      if (x.op == Op::ST || x.op == Op::FSW)
         x.s2 = fRs2(w); // rt is the value stored
      else
         x.d = wr(fRs2(w));
      break;
   case Shape::RT_RS1:
      x.d = wr(fRs2(w)), x.s1 = fRs1(w);
      break;
   case Shape::RT_IMM:
      x.d = wr(fRs2(w)), x.imm = simm;
      break;
   case Shape::BR:
      x.s1 = fRs1(w), x.s2 = fRs2(w), x.imm = static_cast<int32_t>(fImm16(w));
      break;
   case Shape::TGT:
      x.imm = static_cast<int32_t>(fTarget26(w));
      break;
   case Shape::RS1:
      x.s1 = fRs1(w);
      break;
   case Shape::RS1_RT:
      x.s1 = fRs1(w), x.d = wr(fRs2(w));
      break;
   }
   return x;
}

inline float f32(uint32_t u)
{
   return std::bit_cast<float>(u);
}
inline uint32_t u32(float f)
{
   return std::bit_cast<uint32_t>(f);
}

inline uint32_t ftoiw(float f)
{
   if (std::isnan(f))
      return 0;
   if (f >= 2147483648.0f)
      return 0x7FFFFFFFu;
   if (f <= -2147483648.0f)
      return 0x80000000u;
   return static_cast<uint32_t>(static_cast<int32_t>(f)); // truncates toward zero
}

} // namespace

void Cpu::load(const Program &p)
{
   progSize = p.size();
   prog.assign(progSize + 1, Decoded{});
   for (uint32_t i = 0; i < progSize; ++i)
      if (p.filled[i])
         prog[i] = decode(p.words[i]);
}

Stop run(Cpu &c, uint64_t budget)
{
   uint32_t *const R = c.R;
   const Decoded *const P = c.prog.data();
   uint64_t *const cnt = c.count;
   uint32_t pc = c.pc;
   uint32_t fr = c.fr;

   auto done = [&](Stop s)
   {
      c.pc = pc;
      c.fr = fr;
      return s;
   };
   auto trap = [&](const char *msg, uint32_t a)
   {
      c.fault = msg;
      c.faultAddr = a;
      return done(Stop::Fault);
   };
   // n and z flags from an ALU result
#define SET_NZ(r) (fr = (fr & 3u) | (((r) >> 28) & 8u) | ((r) == 0 ? 4u : 0u))
#define CHECK_PC(t)                                                                                \
   if ((t) >= c.progSize)                                                                          \
   return trap("jump target outside the program", (t))
#define EA(I)                                                                                      \
   uint32_t ea = R[(I).s1] + static_cast<uint32_t>((I).imm);                                       \
   if (ea > Memory::MAX_ADDR)                                                                      \
   return trap("data address out of range", ea)

   while (budget)
   {
      --budget;
      const Decoded &I = P[pc];
      ++cnt[static_cast<int>(I.op)];
      switch (I.op)
      {
      // ---------------- Other ----------------
      case Op::CLZ:
         R[I.d] = static_cast<uint32_t>(std::countl_zero(R[I.s1]));
         ++pc;
         break;
      case Op::CNTT0:
         R[I.d] = static_cast<uint32_t>(std::countr_zero(R[I.s1]));
         ++pc;
         break;
      case Op::POP:
         R[I.d] = static_cast<uint32_t>(std::popcount(R[I.s1]));
         ++pc;
         break;
      case Op::BYTR:
         R[I.d] = __builtin_bswap32(R[I.s1]);
         ++pc;
         break;
      case Op::SEXTD:
         R[I.d] =
             static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(R[I.s1] & 0xFFFF)));
         ++pc;
         break;
      case Op::MOVGT:
         if (static_cast<int32_t>(R[I.s1]) > static_cast<int32_t>(R[I.s2]))
            R[I.d] = R[I.s1];
         ++pc;
         break;
      case Op::MOVLT:
         if (static_cast<int32_t>(R[I.s1]) < static_cast<int32_t>(R[I.s2]))
            R[I.d] = R[I.s1];
         ++pc;
         break;
      case Op::NOP:
         ++pc;
         break;

      // ---------------- Memory ----------------
      case Op::LD:
      case Op::FLW:
      {
         EA(I);
         R[I.d] = c.mem.read(ea);
         ++pc;
         break;
      }
      case Op::ST:
      case Op::FSW:
      {
         EA(I);
         c.mem.write(ea, R[I.s2]);
         ++pc;
         break;
      }
      case Op::MOV:
         R[I.d] = R[I.s1];
         ++pc;
         break;
      case Op::LDR:
         R[I.d] = static_cast<uint32_t>(I.imm);
         ++pc;
         break;

      // ---------------- ALU ----------------
      case Op::ADD:
      {
         uint32_t r = R[I.s1] + R[I.s2];
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }
      case Op::SUB:
      {
         uint32_t r = R[I.s1] - R[I.s2];
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }
      case Op::RSB:
      {
         uint32_t r = R[I.s2] - R[I.s1];
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }
      case Op::INC:
      {
         uint32_t r = R[I.s1] + 1;
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }
      case Op::DEC:
      {
         uint32_t r = R[I.s1] - 1;
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }
      case Op::RS:
      {
         uint32_t r = R[I.s1] >> 1;
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }
      case Op::LS:
      {
         uint32_t r = R[I.s1] << 1;
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }
      case Op::AND:
      {
         uint32_t r = R[I.s1] & R[I.s2];
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }
      case Op::OR:
      {
         uint32_t r = R[I.s1] | R[I.s2];
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }
      case Op::NAND:
      {
         uint32_t r = ~(R[I.s1] & R[I.s2]);
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }
      case Op::NOR:
      {
         uint32_t r = ~(R[I.s1] | R[I.s2]);
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }
      case Op::XOR:
      {
         uint32_t r = R[I.s1] ^ R[I.s2];
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }
      case Op::NOT:
      {
         uint32_t r = ~R[I.s1];
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }
      case Op::ADDI:
      {
         uint32_t r = R[I.s1] + static_cast<uint32_t>(I.imm);
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }
      case Op::SUBI:
      {
         uint32_t r = R[I.s1] - static_cast<uint32_t>(I.imm);
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }
      case Op::SFLIP:
      {
         uint32_t r = 0u - R[I.s1];
         R[I.d] = r;
         SET_NZ(r);
         ++pc;
         break;
      }

      // ---------------- Floating point (shared register file) ----------------
      case Op::FADD:
         R[I.d] = u32(f32(R[I.s1]) + f32(R[I.s2]));
         ++pc;
         break;
      case Op::FSUB:
         R[I.d] = u32(f32(R[I.s1]) - f32(R[I.s2]));
         ++pc;
         break;
      case Op::FMULT:
         R[I.d] = u32(f32(R[I.s1]) * f32(R[I.s2]));
         ++pc;
         break;
      case Op::FDIV:
         R[I.d] = u32(f32(R[I.s1]) / f32(R[I.s2]));
         ++pc;
         break;
      case Op::FFMA:
         R[I.d] = u32(std::fmaf(f32(R[I.s1]), f32(R[I.s2]), f32(R[I.s3])));
         ++pc;
         break;
      case Op::FSQRT:
         R[I.d] = u32(std::sqrt(f32(R[I.s1])));
         ++pc;
         break;
      case Op::FNEG:
         R[I.d] = R[I.s1] ^ 0x80000000u;
         ++pc;
         break;
      case Op::FABS:
         R[I.d] = R[I.s1] & 0x7FFFFFFFu;
         ++pc;
         break;
      case Op::FLT:
         fr = (fr & ~2u) | (f32(R[I.s1]) < f32(R[I.s2]) ? 2u : 0u);
         ++pc;
         break;
      case Op::FEQ:
         fr = (fr & ~1u) | (f32(R[I.s1]) == f32(R[I.s2]) ? 1u : 0u);
         ++pc;
         break;
      case Op::FMAX:
      {
         float a = f32(R[I.s1]), b = f32(R[I.s2]);
         R[I.d] = a > b ? R[I.s1] : R[I.s2];
         ++pc;
         break;
      }
      case Op::FTOIW:
         R[I.d] = ftoiw(f32(R[I.s1]));
         ++pc;
         break;
      case Op::IWTOF:
         R[I.d] = u32(static_cast<float>(static_cast<int32_t>(R[I.s1])));
         ++pc;
         break;
      case Op::FMOV:
         R[I.d] = R[I.s1];
         ++pc;
         break;

         // ---------------- Control ----------------
#define BRANCH(cond)                                                                               \
   if (cond)                                                                                       \
   {                                                                                               \
      pc = static_cast<uint32_t>(I.imm);                                                           \
      c.bextArmed = pc;                                                                            \
   }                                                                                               \
   else                                                                                            \
   {                                                                                               \
      ++pc;                                                                                        \
   }                                                                                               \
   break;
      case Op::BEQ:
         BRANCH(R[I.s1] == R[I.s2])
      case Op::BNE:
         BRANCH(R[I.s1] != R[I.s2])
      case Op::BLT:
         BRANCH(static_cast<int32_t>(R[I.s1]) < static_cast<int32_t>(R[I.s2]))
      case Op::BLE:
         BRANCH(static_cast<int32_t>(R[I.s1]) <= static_cast<int32_t>(R[I.s2]))
      case Op::BGT:
         BRANCH(static_cast<int32_t>(R[I.s1]) > static_cast<int32_t>(R[I.s2]))
      case Op::BGE:
         BRANCH(static_cast<int32_t>(R[I.s1]) >= static_cast<int32_t>(R[I.s2]))
#undef BRANCH
      case Op::JUMP:
         c.sr = pc + 1;
         pc = static_cast<uint32_t>(I.imm);
         break;
      case Op::JUMPR:
      {
         uint32_t t = R[I.s1];
         CHECK_PC(t);
         c.sr = pc + 1;
         pc = t;
         break;
      }
      case Op::JLINK:
      {
         uint32_t t = R[I.s1];
         CHECK_PC(t);
         R[I.d] = pc + 1;
         c.sr = pc + 1;
         pc = t;
         break;
      }
      case Op::BEXT: // taken only when we arrived here via a taken branch
         if (c.bextArmed == pc)
         {
            c.bextArmed = 0xFFFFFFFFu;
            c.sr = pc + 1;
            pc = static_cast<uint32_t>(I.imm);
         }
         else
         {
            ++pc;
         }
         break;
      case Op::IN:
      {
         // One line from stdin; EOF / blank / non-number reads as 0 (deterministic).
         if (isatty(STDIN_FILENO))
         {
            std::printf("IN R%u > ", I.s1);
            std::fflush(stdout);
         }
         char buf[64];
         long long v = 0;
         if (std::fgets(buf, sizeof buf, stdin))
            v = std::strtoll(buf, nullptr, 0);
         R[I.d] = static_cast<uint32_t>(v);
         ++pc;
         break;
      }
      case Op::OUT:
         std::printf("OUT: 0x%08X (%d)\n", R[I.s1], static_cast<int32_t>(R[I.s1]));
         std::fflush(stdout);
         ++pc;
         break;
      case Op::RET:
      {
         uint32_t t = c.sr;
         CHECK_PC(t);
         pc = t;
         break;
      }
      case Op::FILL:
         R[I.d] = 0xFFFFFFFFu;
         ++pc;
         break;
      case Op::CLR:
         R[I.d] = 0;
         ++pc;
         break;
      case Op::HALT:
         return done(Stop::Halted);

      case Op::ILLEGAL:
         return trap("illegal instruction / PC outside program", pc);
      }
   }
   return done(Stop::Budget);
#undef SET_NZ
#undef CHECK_PC
#undef EA
}
