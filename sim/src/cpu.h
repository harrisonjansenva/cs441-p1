#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "assembler.h"
#include "isa.h"
#include "memory.h"

enum class Stop
{
   Halted,
   Budget,
   Fault
};

struct Cpu
{
   // 32 architectural registers: R0 = 0, R1 = 1, R2-R31 general purpose
   uint32_t R[32] = {};

   uint32_t pc = 0; // Program counter
   uint32_t fr = 0; // Flag register: bit 3 = n, bit 2 = z, bit 1 = l, bit 0 = e
   uint32_t sr = 0; // Scope register: PC+1 on jumps

   uint64_t count[NUM_OPS] = {};

   const char *fault = nullptr;
   uint32_t faultAddr = 0;

   // Instruction memory
   std::vector<uint32_t> progWords;
   std::vector<uint8_t> progFilled;
   uint32_t progSize = 0;

   Memory mem;

   Cpu()
   {
      R[0] = 0;
      R[1] = 1;
   }

   // Writes to R0 and R1 are ignored
   void setReg(uint32_t reg, uint32_t val)
   {
      if (reg >= 2 && reg < 32)
      {
         R[reg] = val;
      }
   }

   void load(const Program &p);

   uint64_t total() const
   {
      uint64_t sum = 0;
      for (int i = 0; i < NUM_OPS; ++i)
      {
         sum += count[i];
      }
      return sum;
   }
};

// Execute instructions up to the given budget
Stop run(Cpu &cpu, uint64_t budget);
