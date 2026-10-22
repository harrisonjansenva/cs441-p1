// isaSim - Instruction level simulator for CS 441 Project 1.
// Usage: isaSim -<r|s> <assembly file> <memory file> [-d] [--max-instr N]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "assembler.h"
#include "cpu.h"
#include "isa.h"

namespace
{

constexpr uint64_t DEFAULT_MAX_INSTR = 2000000000ull;

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

// Extract file base name from path (stripping directory)
std::string getBaseName(const std::string &path)
{
   size_t slash = path.find_last_of("/\\");
   if (slash == std::string::npos)
   {
      return path;
   }
   return path.substr(slash + 1);
}

// Strip extension from file name
std::string stripExtension(const std::string &name)
{
   size_t dot = name.find_last_of('.');
   if (dot == std::string::npos || dot == 0)
   {
      return name;
   }
   return name.substr(0, dot);
}

// Print architectural state during step mode
void printState(const Cpu &c, const Program &p, const std::string &executed)
{
   if (!executed.empty())
   {
      std::printf("executed: %s\n", executed.c_str());
   }

   if (c.pc < c.progSize && p.filled[c.pc])
   {
      std::printf("PC = 0x%07X   next: %s\n", c.pc, disassemble(p.words[c.pc]).c_str());
   }
   else
   {
      std::printf("PC = 0x%07X   next: <no instruction>\n", c.pc);
   }

   std::printf("FR = 0x%X (n=%u z=%u l=%u e=%u)   SR = 0x%07X\n", c.fr, (c.fr >> 3) & 1,
               (c.fr >> 2) & 1, (c.fr >> 1) & 1, c.fr & 1, c.sr);

   for (int i = 0; i < 32; ++i)
   {
      std::printf("R%-2d=%08X%s", i, c.R[i], (i % 4 == 3) ? "\n" : "  ");
   }
   std::printf("Total instructions executed: %llu\n", static_cast<unsigned long long>(c.total()));
}

// Format instruction count table grouped by instruction class
std::string buildCountsReport(const Cpu &c)
{
   std::string report;
   char buffer[128];
   Cls currentClass = Cls::Control;
   bool isFirst = true;

   for (int i = 0; i < NUM_OPS; ++i)
   {
      const InstrDesc &d = ISA[i];
      if (isFirst || d.cls != currentClass)
      {
         currentClass = d.cls;
         isFirst = false;
         std::snprintf(buffer, sizeof(buffer), "# %s\n", className(currentClass));
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

} // namespace

int main(int argc, char **argv)
{
   bool stepMode = false;
   bool haveMode = false;
   bool dumpDisasm = false;
   uint64_t maxInstructions = DEFAULT_MAX_INSTR;
   std::string asmPath;
   std::string memPath;
   int positionalArgs = 0;

   for (int i = 1; i < argc; ++i)
   {
      std::string arg = argv[i];
      if (arg == "-r" || arg == "-s")
      {
         if (haveMode)
         {
            printUsage();
            return 2;
         }
         haveMode = true;
         stepMode = (arg == "-s");
      }
      else if (arg == "-d")
      {
         dumpDisasm = true;
      }
      else if (arg == "--max-instr")
      {
         if (i + 1 >= argc)
         {
            printUsage();
            return 2;
         }
         i++;
         maxInstructions = std::strtoull(argv[i], nullptr, 0);
      }
      else if (!arg.empty() && arg[0] == '-')
      {
         std::fprintf(stderr, "isaSim: unknown option '%s'\n", arg.c_str());
         printUsage();
         return 2;
      }
      else if (positionalArgs == 0)
      {
         asmPath = arg;
         positionalArgs++;
      }
      else if (positionalArgs == 1)
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

   if (!haveMode || positionalArgs != 2)
   {
      printUsage();
      return 2;
   }

   // Assemble the source file
   Program prog;
   std::vector<std::string> assembleErrors = assemble(asmPath, prog);
   if (!assembleErrors.empty())
   {
      for (const std::string &err : assembleErrors)
      {
         std::fprintf(stderr, "%s\n", err.c_str());
      }
      std::fprintf(stderr, "isaSim: assembly failed (%zu error%s)\n", assembleErrors.size(),
                   assembleErrors.size() == 1 ? "" : "s");
      return 1;
   }

   Cpu cpu;
   std::string memError = cpu.mem.loadMemFile(memPath);
   if (!memError.empty())
   {
      std::fprintf(stderr, "isaSim: %s\n", memError.c_str());
      return 1;
   }
   cpu.load(prog);

   if (dumpDisasm)
   {
      for (uint32_t a = 0; a < prog.size(); ++a)
      {
         if (prog.filled[a])
         {
            std::printf("0x%07X: %08X  %s\n", a, prog.words[a], disassemble(prog.words[a]).c_str());
         }
      }
   }

   Stop stopReason = Stop::Budget;

   if (!stepMode)
   {
      // Run straight through to HALT
      stopReason = run(cpu, maxInstructions);
   }
   else
   {
      // Step-by-step interactive mode
      std::printf("Step mode. Enter = 1 step, N = N steps, c = continue to end, q = quit.\n");
      printState(cpu, prog, "");

      uint64_t remainingBudget = maxInstructions;
      bool quit = false;

      while (true)
      {
         std::printf("\n[step] > ");
         std::fflush(stdout);

         std::string line;
         bool eof = !std::getline(std::cin, line);
         uint64_t stepsToRun = 1;

         if (eof || line == "c" || line == "C")
         {
            stepsToRun = remainingBudget;
         }
         else if (line == "q" || line == "Q")
         {
            quit = true;
            break;
         }
         else if (!line.empty())
         {
            char *end = nullptr;
            unsigned long long v = std::strtoull(line.c_str(), &end, 0);
            if (end == line.c_str() || v == 0)
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

         uint32_t pcBefore = cpu.pc;
         uint64_t countBefore = cpu.total();
         stopReason = run(cpu, stepsToRun);

         uint64_t executedCount = cpu.total() - countBefore;
         remainingBudget -= executedCount;

         std::string executedDesc;
         if (executedCount == 1 && pcBefore < prog.size() && prog.filled[pcBefore])
         {
            executedDesc = disassemble(prog.words[pcBefore]);
         }
         else if (executedCount > 1)
         {
            executedDesc = std::to_string(executedCount) + " instructions";
         }

         std::printf("\n");
         printState(cpu, prog, executedDesc);

         if (stopReason != Stop::Budget || remainingBudget == 0 || eof)
         {
            break;
         }
      }

      if (quit)
      {
         stopReason = Stop::Budget;
      }
   }

   int exitCode = 0;
   if (stopReason == Stop::Halted)
   {
      std::printf("\nHALT reached.\n");
   }
   else if (stopReason == Stop::Fault)
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
   std::string report = buildCountsReport(cpu);
   std::printf("\nInstruction counts:\n%s", report.c_str());

   std::string countPath = stripExtension(getBaseName(asmPath)) + ".instrCount";
   FILE *countFile = std::fopen(countPath.c_str(), "w");
   if (countFile)
   {
      std::fputs(report.c_str(), countFile);
      std::fclose(countFile);
   }
   else
   {
      std::fprintf(stderr, "isaSim: cannot write %s\n", countPath.c_str());
      if (exitCode == 0)
      {
         exitCode = 1;
      }
   }

   std::string outPath = getBaseName(memPath) + ".out";
   std::string writeErr = cpu.mem.writeMemOut(outPath);
   if (!writeErr.empty())
   {
      std::fprintf(stderr, "isaSim: %s\n", writeErr.c_str());
      if (exitCode == 0)
      {
         exitCode = 1;
      }
   }

   std::printf("\nWrote %s and %s\n", countPath.c_str(), outPath.c_str());
   return exitCode;
}
