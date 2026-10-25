////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file isaSim - Instruction level simulator for CS 441 Project 1.
/// Usage: isaSim -<r|s> <assembly file> <memory file> [-d] [--max-instr N]
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "assembler.h"
#include "cpu.h"
#include "isa.h"
#include "text.h"

using namespace Simulator;

namespace
{
constexpr std::uint64_t DEFAULT_MAX_INSTR{2000000000ull};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief tell the user how to run the simulator.
////////////////////////////////////////////////////////////////////////////////////////////////////
void printUsage()
{
   std::fputs(
       "usage: isaSim -<r|s> <assembly file> <memory file> [options]\n"
       "  -r              run straight through to HALT\n"
       "  -s              step-by-step (Enter = 1 step, N = N steps, c = continue, q = quit)\n"
       "  -d              also print the assembled program (address, encoding, disassembly)\n"
       "  --max-instr N   abort after N instructions (default 2000000000)\n",
       stderr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief print each error on its own line to stderr.
////////////////////////////////////////////////////////////////////////////////////////////////////
void printErrors(const Errors &errors)
{
   for (const std::string &err : errors)
   {
      std::fprintf(stderr, "%s\n", err.c_str());
   }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Print architectural state during step mode
////////////////////////////////////////////////////////////////////////////////////////////////////
void printState(const Cpu &c, const std::string &executed)
{
   if (!executed.empty())
   {
      std::printf("executed: %s\n", executed.c_str());
   }

   if (c.program.isFilled(c.pc))
   {
      std::printf("PC = 0x%07X   next: %s\n", c.pc,
                  disassemble(c.program.entries[c.pc].word).c_str());
   }
   else
   {
      std::printf("PC = 0x%07X   next: <no instruction>\n", c.pc);
   }

   std::printf("FR = 0x%X (n=%u z=%u l=%u e=%u)   SR = 0x%07X\n", c.fr, (c.fr >> 3) & 1,
               (c.fr >> 2) & 1, (c.fr >> 1) & 1, c.fr & 1, c.sr);

   for (int i{0}; i < 32; ++i)
   {
      std::printf("R%-2d=%08X%s", i, c.R[i], (3 == i % 4) ? "\n" : "  ");
   }
   std::printf("Total instructions executed: %llu\n", static_cast<unsigned long long>(c.total()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Format instruction count table grouped by instruction type
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string buildCountsReport(const Cpu &c)
{
   std::string report;
   char buffer[128];
   Instr_type currentType{Instr_type::Control};
   bool isFirst{true};

   for (std::size_t i{0}; i < NUM_OPS; ++i)
   {
      const InstrDesc &d{ISA[i]};
      if (isFirst || d.type != currentType)
      {
         currentType = d.type;
         isFirst = false;
         std::snprintf(buffer, sizeof(buffer), "# %s\n", typeName(currentType));
         report += buffer;
      }
      std::snprintf(buffer, sizeof(buffer), "%s: %llu\n", d.name,
                    static_cast<unsigned long long>(c.count[i]));
      report += buffer;
   }

   std::snprintf(buffer, sizeof(buffer), "TOTAL: %llu\n",
                 static_cast<unsigned long long>(c.total()));
   report += buffer;
   return report;
}

} // anonymous namespace

int main(int argc, char **argv)
{
   bool stepMode{false};
   bool haveMode{false};
   bool dumpDisasm{false};
   std::uint64_t maxInstructions{DEFAULT_MAX_INSTR};
   std::string asmPath;
   std::string memPath;
   int positionalArgs{0};

   for (int i{1}; i < argc; ++i)
   {
      std::string arg{argv[i]};
      if ("-r" == arg || "-s" == arg)
      {
         if (haveMode)
         {
            printUsage();
            return 2;
         }
         haveMode = true;
         stepMode = ("-s" == arg);
      }
      else if ("-d" == arg)
      {
         dumpDisasm = true;
      }
      else if ("--max-instr" == arg)
      {
         if (i + 1 >= argc)
         {
            printUsage();
            return 2;
         }
         i++;
         maxInstructions = std::strtoull(argv[i], nullptr, 0);
      }
      else if (!arg.empty() && '-' == arg[0])
      {
         std::fprintf(stderr, "isaSim: unknown option '%s'\n", arg.c_str());
         printUsage();
         return 2;
      }
      else if (0 == positionalArgs)
      {
         asmPath = arg;
         positionalArgs++;
      }
      else if (1 == positionalArgs)
      {
         memPath = arg;
         positionalArgs++;
      }
      else
      {
         printUsage();
         return 2;
      }
   }

   if (!haveMode || 2 != positionalArgs)
   {
      printUsage();
      return 2;
   }

   Cpu cpu;

   // Assemble the source file
   Errors assembleErrors{assemble(asmPath, cpu.program)};
   if (!assembleErrors.empty())
   {
      printErrors(assembleErrors);
      std::fprintf(stderr, "isaSim: assembly failed (%zu error%s)\n", assembleErrors.size(),
                   (1 == assembleErrors.size()) ? "" : "s");
      return 1;
   }

   Errors memErrors{cpu.mem.loadMem(memPath)};
   if (!memErrors.empty())
   {
      printErrors(memErrors);
      return 1;
   }

   if (dumpDisasm)
   {
      for (std::uint32_t a{0}; a < cpu.program.size(); ++a)
      {
         if (cpu.program.isFilled(a))
         {
            std::uint32_t word{cpu.program.entries[a].word};
            std::printf("0x%07X: %08X  %s\n", a, word, disassemble(word).c_str());
         }
      }
   }

   Stop stopReason{Stop::Budget};

   if (!stepMode)
   {
      // Run straight through to HALT
      stopReason = run(cpu, maxInstructions);
   }
   else
   {
      // Step-by-step interactive mode
      std::printf("Step mode. Enter = 1 step, N = N steps, c = continue to end, q = quit.\n");
      printState(cpu, "");

      std::uint64_t remainingBudget{maxInstructions};
      bool quit{false};

      while (true)
      {
         std::printf("\n[step] > ");
         std::fflush(stdout);

         std::string line;
         bool eof{!std::getline(std::cin, line)};
         std::uint64_t stepsToRun{1};

         if (eof || "c" == line || "C" == line)
         {
            stepsToRun = remainingBudget;
         }
         else if ("q" == line || "Q" == line)
         {
            quit = true;
            break;
         }
         else if (!line.empty())
         {
            char *end{nullptr};
            unsigned long long v{std::strtoull(line.c_str(), &end, 0)};
            if (end == line.c_str() || 0 == v)
            {
               std::printf("?\n");
               continue;
            }
            stepsToRun = v;
         }

         if (stepsToRun > remainingBudget)
         {
            stepsToRun = remainingBudget;
         }

         std::uint32_t pcBefore{cpu.pc};
         std::uint64_t countBefore{cpu.total()};
         stopReason = run(cpu, stepsToRun);

         std::uint64_t executedCount{cpu.total() - countBefore};
         remainingBudget -= executedCount;

         std::string executedDesc;
         if (1 == executedCount && cpu.program.isFilled(pcBefore))
         {
            executedDesc = disassemble(cpu.program.entries[pcBefore].word);
         }
         else if (1 < executedCount)
         {
            executedDesc = std::to_string(executedCount) + " instructions";
         }

         std::printf("\n");
         printState(cpu, executedDesc);

         if (Stop::Budget != stopReason || 0 == remainingBudget || eof)
         {
            break;
         }
      }

      if (quit)
      {
         stopReason = Stop::Budget;
      }
   }

   int exitCode{0};
   if (Stop::Halted == stopReason)
   {
      std::printf("\nHALT reached.\n");
   }
   else if (Stop::Fault == stopReason)
   {
      std::fprintf(stderr, "\nisaSim: runtime fault at PC=0x%07X: %s (0x%X)\n", cpu.pc, cpu.fault,
                   cpu.faultAddr);
      exitCode = 3;
   }
   else
   {
      std::fprintf(stderr, "\nisaSim: stopped before HALT after %llu instructions\n",
                   static_cast<unsigned long long>(cpu.total()));
      exitCode = 4;
   }

   // Write output reports
   std::string report{buildCountsReport(cpu)};
   std::printf("\nInstruction counts:\n%s", report.c_str());

   std::string countPath{std::filesystem::path{asmPath}.stem().string() + ".instrCount"};
   std::ofstream countFile{countPath};
   if (countFile.is_open())
   {
      countFile << report;
   }
   else
   {
      std::fprintf(stderr, "isaSim: cannot write %s\n", countPath.c_str());
      if (0 == exitCode)
      {
         exitCode = 1;
      }
   }

   std::string outPath{std::filesystem::path{memPath}.filename().string() + ".out"};
   Errors writeErrors{cpu.mem.writeMem(outPath)};
   if (!writeErrors.empty())
   {
      printErrors(writeErrors);
      if (0 == exitCode)
      {
         exitCode = 1;
      }
   }

   std::printf("\nWrote %s and %s\n", countPath.c_str(), outPath.c_str());
   return exitCode;
}
