// cpu.h - architectural state and the one and only instruction execution loop.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "assembler.h"
#include "isa.h"
#include "memory.h"

// An instruction after one-time decoding. Execution never looks at raw bits.
// `d` is the destination register; writes to R0/R1 are redirected at decode
// time to the scratch slot R[SINK], so the hot loop needs no "is this R0?" test.
struct Decoded
{
   Op op = Op::ILLEGAL;
   uint8_t d = 0, s1 = 0, s2 = 0, s3 = 0;
   int32_t imm = 0; // sign-extended imm16, branch target, or J target26
};

enum class Stop
{
   Halted,
   Budget,
   Fault
};

struct Cpu
{
   static constexpr int SINK = 32;
   uint32_t R[33] = {}; // R[0]=0, R[1]=1, R[2..31] general, R[32] = write sink
   uint32_t pc = 0;
   uint32_t fr = 0; // flag register: bit3 n, bit2 z, bit1 l, bit0 e
   uint32_t sr = 0; // scope register: PC+1 of the last jump
   uint32_t bextArmed = 0xFFFFFFFFu;
   uint64_t count[NUM_OPS + 1] = {};
   const char *fault = nullptr;
   uint32_t faultAddr = 0;

   std::vector<Decoded> prog; // prog.size() == progSize + 1 (trailing ILLEGAL sentinel)
   uint32_t progSize = 0;
   Memory mem;

   Cpu()
   {
      R[1] = 1;
   }
   void load(const Program &p);
   uint64_t total() const
   {
      uint64_t t = 0;
      for (int i = 0; i < NUM_OPS; ++i)
         t += count[i];
      return t;
   }
};

// Execute up to `budget` instructions. Used by both run mode and step mode.
Stop run(Cpu &cpu, uint64_t budget);
