/**
 * @file main.cpp
 * @brief Command-line driver for the CS 441 P1 instruction-level simulator.
 *
 * Usage: `isaSim -<r|s> <assembly file> <memory file> [-d] [--max-instr N]`
 */
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
   size_t s = path.find_last_of("/\\");
   return s == std::string::npos ? path : path.substr(s + 1);
}

/// File name without its last extension.
std::string stripExt(const std::string &name)
{
   size_t d = name.find_last_of('.');
   return d == std::string::npos || d == 0 ? name : name.substr(0, d);
}

/**
 * @brief Print PC, flags, SR, R0-R31 and the instruction total (one step-mode display).
 * @param c        Machine state.
 * @param p        Assembled program, used to disassemble the next instruction.
 * @param executed Description of what just ran; omitted if empty.
 */
void printState(const Cpu &c, const Program &p, const std::string &executed)
{
   if (!executed.empty())
      std::printf("executed: %s\n", executed.c_str());
   if (c.pc < c.progSize && p.filled[c.pc])
      std::printf("PC = 0x%07X   next: %s\n", c.pc, disassemble(p.words[c.pc]).c_str());
   else
      std::printf("PC = 0x%07X   next: <no instruction>\n", c.pc);
   std::printf("FR = 0x%X (n=%u z=%u l=%u e=%u)   SR = 0x%07X\n", c.fr, (c.fr >> 3) & 1,
               (c.fr >> 2) & 1, (c.fr >> 1) & 1, c.fr & 1, c.sr);
   for (int i = 0; i < 32; ++i)
   {
      std::printf("R%-2d=%08X%s", i, c.R[i], (i % 4 == 3) ? "\n" : "  ");
   }
   std::printf("Total instructions executed: %llu\n", static_cast<unsigned long long>(c.total()));
}

/// Per-instruction execution counts, grouped by class, ending with TOTAL.
std::string countsReport(const Cpu &c)
{
   std::string s;
   char b[96];
   Cls cur = Cls::Control;
   bool first = true;
   for (int i = 0; i < NUM_OPS; ++i)
   {
      const InstrDesc &d = ISA[i];
      if (first || d.cls != cur)
      {
         cur = d.cls;
         first = false;
         std::snprintf(b, sizeof b, "# %s\n", className(cur));
         s += b;
      }
      std::snprintf(b, sizeof b, "%s: %llu\n", d.name, static_cast<unsigned long long>(c.count[i]));
      s += b;
   }
   std::snprintf(b, sizeof b, "TOTAL: %llu\n", static_cast<unsigned long long>(c.total()));
   s += b;
   return s;
}

} // namespace

int main(int argc, char **argv)
{
   bool step = false, haveMode = false, dump = false;
   uint64_t maxInstr = DEFAULT_MAX_INSTR;
   std::string asmPath, memPath;
   int positional = 0;
   for (int i = 1; i < argc; ++i)
   {
      std::string a = argv[i];
      if (a == "-r" || a == "-s")
      {
         if (haveMode)
         {
            usage();
            return 2;
         }
         haveMode = true;
         step = a == "-s";
      }
      else if (a == "-d")
      {
         dump = true;
      }
      else if (a == "--max-instr")
      {
         if (i + 1 >= argc)
         {
            usage();
            return 2;
         }
         maxInstr = std::strtoull(argv[++i], nullptr, 0);
      }
      else if (!a.empty() && a[0] == '-')
      {
         std::fprintf(stderr, "isaSim: unknown option '%s'\n", a.c_str());
         usage();
         return 2;
      }
      else if (positional == 0)
      {
         asmPath = a, ++positional;
      }
      else if (positional == 1)
      {
         memPath = a, ++positional;
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
   std::vector<std::string> errs = assemble(asmPath, prog);
   if (!errs.empty())
   {
      for (const std::string &e : errs)
         std::fprintf(stderr, "%s\n", e.c_str());
      std::fprintf(stderr, "isaSim: assembly failed (%zu error%s)\n", errs.size(),
                   errs.size() == 1 ? "" : "s");
      return 1;
   }

   Cpu cpu;
   std::string merr = cpu.mem.loadMemFile(memPath);
   if (!merr.empty())
   {
      std::fprintf(stderr, "isaSim: %s\n", merr.c_str());
      return 1;
   }
   cpu.load(prog);

   if (dump)
   {
      for (uint32_t a = 0; a < prog.size(); ++a)
         if (prog.filled[a])
            std::printf("0x%07X: %08X  %s\n", a, prog.words[a], disassemble(prog.words[a]).c_str());
   }

   Stop why = Stop::Budget;
   if (!step)
   {
      why = run(cpu, maxInstr);
   }
   else
   {
      std::printf("Step mode. Enter = 1 step, N = N steps, c = continue to end, q = quit.\n");
      printState(cpu, prog, "");
      uint64_t remaining = maxInstr;
      bool quit = false;
      uint64_t n = 0;
      for (;;)
      {
         std::printf("\n[step] > ");
         std::fflush(stdout);
         std::string line;
         bool eof = !std::getline(std::cin, line);
         uint64_t want = 1;
         if (eof || line == "c" || line == "C")
            want = remaining;
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
            want = v;
         }
         if (want > remaining)
            want = remaining;
         uint32_t before = cpu.pc;
         uint64_t t0 = cpu.total();
         why = run(cpu, want);
         n = cpu.total() - t0;
         remaining -= n;
         std::string executed;
         if (n == 1 && before < prog.size() && prog.filled[before])
            executed = disassemble(prog.words[before]);
         else if (n > 1)
            executed = std::to_string(n) + " instructions";
         std::printf("\n");
         printState(cpu, prog, executed);
         if (why != Stop::Budget || remaining == 0)
            break;
         if (eof)
            break;
      }
      if (quit)
         why = Stop::Budget;
   }

   int rc = 0;
   if (why == Stop::Halted)
      std::printf("\nHALT reached.\n");
   else if (why == Stop::Fault)
   {
      std::fprintf(stderr, "\nisaSim: runtime fault at PC=0x%07X: %s (0x%X)\n", cpu.pc, cpu.fault,
                   cpu.faultAddr);
      rc = 3;
   }
   else
   {
      std::fprintf(stderr, "\nisaSim: stopped before HALT after %llu instructions\n",
                   static_cast<unsigned long long>(cpu.total()));
      rc = 4;
   }

   // end-of-run outputs, written to the current directory
   std::string report = countsReport(cpu);
   std::printf("\nInstruction counts:\n%s", report.c_str());

   std::string countPath = stripExt(baseName(asmPath)) + ".instrCount";
   if (FILE *f = std::fopen(countPath.c_str(), "w"))
   {
      std::fputs(report.c_str(), f);
      std::fclose(f);
   }
   else
   {
      std::fprintf(stderr, "isaSim: cannot write %s\n", countPath.c_str());
      rc = rc ? rc : 1;
   }
   std::string outPath = baseName(memPath) + ".out";
   std::string werr = cpu.mem.writeMemOut(outPath);
   if (!werr.empty())
   {
      std::fprintf(stderr, "isaSim: %s\n", werr.c_str());
      rc = rc ? rc : 1;
   }
   std::printf("\nWrote %s and %s\n", countPath.c_str(), outPath.c_str());
   return rc;
}
