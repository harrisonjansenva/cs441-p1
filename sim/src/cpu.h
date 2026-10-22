/**
 * @file cpu.h
 * @brief Architectural state and the instruction execution loop.
 */
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "assembler.h"
#include "isa.h"
#include "memory.h"

/**
 * @brief An instruction after one-time decoding.
 *
 * Execution never looks at raw instruction bits. Writes to R0/R1 are
 * redirected at decode time to the scratch slot Cpu::SINK, so the execution
 * loop does not need an "is the destination R0?" test.
 */
struct Decoded
{
   Op op = Op::ILLEGAL;
   uint8_t d = 0;  ///< destination register (or Cpu::SINK for R0/R1)
   uint8_t s1 = 0; ///< first source register
   uint8_t s2 = 0; ///< second source register
   uint8_t s3 = 0; ///< third source register (FFMA only)
   int32_t imm = 0; ///< sign-extended imm16, branch target, or 26-bit jump target
};

/** @brief Why run() returned. */
enum class Stop
{
   Halted, ///< a HALT instruction executed
   Budget, ///< the instruction budget ran out first
   Fault   ///< see Cpu::fault and Cpu::faultAddr
};

/** @brief Registers, program, data memory and statistics of the simulated machine. */
struct Cpu
{
   static constexpr int SINK = 32; ///< index of the discard slot in #R

   /// R[0] = 0, R[1] = 1, R[2..31] general purpose, R[32] = write sink
   uint32_t R[33] = {};
   uint32_t pc = 0;                  ///< program counter (instruction index)
   uint32_t fr = 0;                  ///< flag register: bit 3 n, bit 2 z, bit 1 l, bit 0 e
   uint32_t sr = 0;                  ///< scope register: PC+1 of the last jump
   uint64_t count[NUM_OPS + 1] = {}; ///< times each Op executed (last slot is Op::ILLEGAL)
   const char *fault = nullptr;      ///< message for the last Stop::Fault
   uint32_t faultAddr = 0;           ///< offending PC, jump target or data address

   std::vector<Decoded> prog; ///< decoded program plus one trailing ILLEGAL sentinel
   uint32_t progSize = 0;     ///< number of instruction slots, excluding the sentinel
   Memory mem;                ///< data memory

   Cpu()
   {
      R[1] = 1;
   }

   /**
    * @brief Decode an assembled program into #prog.
    * @param p Output of assemble(). Unfilled addresses become illegal instructions.
    */
   void load(const Program &p);

   /** @brief Total number of instructions executed so far. */
   uint64_t total() const
   {
      uint64_t t = 0;
      for (int i = 0; i < NUM_OPS; ++i)
         t += count[i];
      return t;
   }
};

/**
 * @brief Execute instructions until HALT, a fault, or the budget runs out.
 *
 * Both run mode and step mode call this; step mode passes a small budget.
 *
 * @param cpu    Machine to run; its pc, registers and counters are updated.
 * @param budget Maximum number of instructions to execute.
 * @return Why execution stopped.
 */
Stop run(Cpu &cpu, uint64_t budget);
