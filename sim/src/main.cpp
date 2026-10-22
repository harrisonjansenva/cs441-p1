/**
 * @file main.cpp
 * @brief Command-line driver for the CS 441 P1 instruction-level simulator.
 *
 * Usage: `isaSim -<r|s> <assembly file> <memory file> [-d] [--max-instr N]`
 */
#include <cinttypes>
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

/// Instruction budget when --max-instr is not given.
constexpr uint64_t DEFAULT_MAX_INSTR = 2000000000ull;

/// Print the usage message to stderr.
void usage()
{
   std::fputs(
       "usage: isaSim -<r|s> <assembly file> <memory file> [options]\n"
       "  -r              run straight through to HALT\n"
       "  -s              step-by-step (Enter = 1 step, N = N steps, c = continue, q = quit)\n"
       "  -d              also print the assembled program (address, encoding, disassembly)\n"
       "  --max-instr N   abort after N instructions (default 2000000000)\n",
       stderr);
}

/// File name with any directory part removed.
std::string baseName(const std::string &path)
{
   size_t slash = path.find_last_of("/\\");
   return slash == std::string::npos ? path : path.substr(slash + 1);
}

/// File name without its last extension.
std::string stripExt(const std::string &name)
{
   size_t dot = name.find_last_of('.');
   return dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
}

/**
 * @brief Print PC, flags, SR, R0-R31 and the instruction total (one step-mode display).
 * @param cpu      Machine state.
 * @param prog     Assembled program, used to disassemble the next instruction.
 * @param executed Description of what just ran; omitted if empty.
 */
void printState(const Cpu &cpu, const Program &prog, const std::string &executed)
{
   if (!executed.empty())
      std::printf("executed: %s\n", executed.c_str());

   if (cpu.pc < cpu.progSize && prog.filled[cpu.pc])
      std::printf("PC = 0x%07X   next: %s\n", cpu.pc, disassemble(prog.words[cpu.pc]).c_str());
   else
      std::printf("PC = 0x%07X   next: <no instruction>\n", cpu.pc);

   std::printf("FR = 0x%X (n=%u z=%u l=%u e=%u)   SR = 0x%07X\n", cpu.fr, (cpu.fr >> 3) & 1,
               (cpu.fr >> 2) & 1, (cpu.fr >> 1) & 1, cpu.fr & 1, cpu.sr);
   for (int i = 0; i < 32; ++i)
      std::printf("R%-2d=%08X%s", i, cpu.R[i], (i % 4 == 3) ? "\n" : "  ");
   std::printf("Total instructions executed: %" PRIu64 "\n", cpu.total());
}

/// Per-instruction execution counts, grouped by class, ending with TOTAL.
std::string countsReport(const Cpu &cpu)
{
   std::string report;
   char line[96];
   Cls currentClass = Cls::Control;
   bool firstRow = true;
   for (int i = 0; i < NUM_OPS; ++i)
   {
      const InstrDesc &info = ISA[i];
      if (firstRow || info.cls != currentClass)
      {
         currentClass = info.cls;
         firstRow = false;
         std::snprintf(line, sizeof line, "# %s\n", className(currentClass));
         report += line;
      }
      std::snprintf(line, sizeof line, "%s: %" PRIu64 "\n", info.name, cpu.count[i]);
      report += line;
   }
   std::snprintf(line, sizeof line, "TOTAL: %" PRIu64 "\n", cpu.total());
   report += line;
   return report;
}

/**
 * @brief Run the interactive step-by-step prompt.
 *
 * Enter runs one instruction, a number runs that many, `c` runs to the end and
 * `q` quits. End of input behaves like `c`.
 *
 * @param cpu      Machine to step.
 * @param prog     Assembled program, used for display.
 * @param maxInstr Instruction budget for the whole session.
 * @return Why execution stopped; quitting early reports Stop::Budget.
 */
Stop stepThrough(Cpu &cpu, const Program &prog, uint64_t maxInstr)
{
   std::printf("Step mode. Enter = 1 step, N = N steps, c = continue to end, q = quit.\n");
   printState(cpu, prog, "");

   Stop why = Stop::Budget;
   uint64_t remaining = maxInstr;
   for (;;)
   {
      std::printf("\n[step] > ");
      std::fflush(stdout);
      std::string line;
      bool endOfInput = !std::getline(std::cin, line);

      uint64_t steps = 1;
      if (endOfInput || line == "c" || line == "C")
      {
         steps = remaining;
      }
      else if (line == "q" || line == "Q")
      {
         return Stop::Budget;
      }
      else if (!line.empty())
      {
         char *end = nullptr;
         unsigned long long requested = std::strtoull(line.c_str(), &end, 0);
         if (end == line.c_str() || requested == 0)
         {
            std::printf("?\n");
            continue;
         }
         steps = requested;
      }
      if (steps > remaining)
         steps = remaining;

      uint32_t pcBefore = cpu.pc;
      uint64_t totalBefore = cpu.total();
      why = cpu.run(steps);
      uint64_t ran = cpu.total() - totalBefore;
      remaining -= ran;

      std::string executed;
      if (ran == 1 && pcBefore < prog.size() && prog.filled[pcBefore])
         executed = disassemble(prog.words[pcBefore]);
      else if (ran > 1)
         executed = std::to_string(ran) + " instructions";

      std::printf("\n");
      printState(cpu, prog, executed);
      if (why != Stop::Budget || remaining == 0 || endOfInput)
         break;
   }
   return why;
}

} // namespace

int main(int argc, char **argv)
{
   bool stepMode = false;
   bool haveMode = false;
   bool dumpProgram = false;
   uint64_t maxInstr = DEFAULT_MAX_INSTR;
   std::string asmPath;
   std::string memPath;
   int positional = 0;
   for (int i = 1; i < argc; ++i)
   {
      std::string arg = argv[i];
      if (arg == "-r" || arg == "-s")
      {
         if (haveMode)
         {
            usage();
            return 2;
         }
         haveMode = true;
         stepMode = arg == "-s";
      }
      else if (arg == "-d")
      {
         dumpProgram = true;
      }
      else if (arg == "--max-instr")
      {
         if (i + 1 >= argc)
         {
            usage();
            return 2;
         }
         maxInstr = std::strtoull(argv[++i], nullptr, 0);
      }
      else if (!arg.empty() && arg[0] == '-')
      {
         std::fprintf(stderr, "isaSim: unknown option '%s'\n", arg.c_str());
         usage();
         return 2;
      }
      else if (positional == 0)
      {
         asmPath = arg;
         ++positional;
      }
      else if (positional == 1)
      {
         memPath = arg;
         ++positional;
      }
      else
      {
         usage();
         return 2;
      }
   }
   if (!haveMode || positional != 2)
   {
      usage();
      return 2;
   }

   Program prog;
   std::vector<std::string> errors = assemble(asmPath, prog);
   if (!errors.empty())
   {
      for (const std::string &error : errors)
         std::fprintf(stderr, "%s\n", error.c_str());
      std::fprintf(stderr, "isaSim: assembly failed (%zu error%s)\n", errors.size(),
                   errors.size() == 1 ? "" : "s");
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

   if (dumpProgram)
   {
      for (uint32_t addr = 0; addr < prog.size(); ++addr)
      {
         if (prog.filled[addr])
         {
            std::printf("0x%07X: %08X  %s\n", addr, prog.words[addr],
                        disassemble(prog.words[addr]).c_str());
         }
      }
   }

   Stop why = stepMode ? stepThrough(cpu, prog, maxInstr) : cpu.run(maxInstr);

   int exitCode = 0;
   if (why == Stop::Halted)
   {
      std::printf("\nHALT reached.\n");
   }
   else if (why == Stop::Fault)
   {
      std::fprintf(stderr, "\nisaSim: runtime fault at PC=0x%07X: %s (0x%X)\n", cpu.pc, cpu.fault,
                   cpu.faultAddr);
      exitCode = 3;
   }
   else
   {
      std::fprintf(stderr, "\nisaSim: stopped before HALT after %" PRIu64 " instructions\n",
                   cpu.total());
      exitCode = 4;
   }

   // end-of-run outputs, written to the current directory
   std::string report = countsReport(cpu);
   std::printf("\nInstruction counts:\n%s", report.c_str());

   std::string countPath = stripExt(baseName(asmPath)) + ".instrCount";
   if (FILE *file = std::fopen(countPath.c_str(), "w"))
   {
      std::fputs(report.c_str(), file);
      std::fclose(file);
   }
   else
   {
      std::fprintf(stderr, "isaSim: cannot write %s\n", countPath.c_str());
      if (exitCode == 0)
         exitCode = 1;
   }

   std::string outPath = baseName(memPath) + ".out";
   std::string writeError = cpu.mem.writeMemOut(outPath);
   if (!writeError.empty())
   {
      std::fprintf(stderr, "isaSim: %s\n", writeError.c_str());
      if (exitCode == 0)
         exitCode = 1;
   }
   std::printf("\nWrote %s and %s\n", countPath.c_str(), outPath.c_str());
   return exitCode;
}
