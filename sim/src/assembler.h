// assembler.h - two-pass assembler: text -> real 32-bit instruction words.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Program
{
   std::vector<uint32_t> words; // indexed by instruction address (PC)
   std::vector<uint8_t> filled; // 1 if an instruction was assembled there
   std::vector<int> srcLine;    // source line per address (0 if unfilled)
   uint32_t size() const
   {
      return static_cast<uint32_t>(words.size());
   }
};

// Returns the list of error messages ("file:line: message"); empty on success.
std::vector<std::string> assemble(const std::string &path, Program &out);
