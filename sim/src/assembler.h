/**
 * @file assembler.h
 * @brief Two-pass assembler: text to 32-bit instruction words.
 */
#pragma once
#include <cstdint>
#include <string>
#include <vector>

/** @brief An assembled program, indexed by instruction address. */
struct Program
{
   std::vector<uint32_t> words; ///< encoded instruction at each address (PC)
   std::vector<uint8_t> filled; ///< 1 if an instruction was assembled at that address
   std::vector<int> srcLine;    ///< source line per address (0 if unfilled)

   /** @brief Number of addresses covered, including unfilled gaps. */
   uint32_t size() const
   {
      return static_cast<uint32_t>(words.size());
   }
};

/**
 * @brief Assemble a source file, resolving labels to absolute addresses.
 * @param path Assembly source file.
 * @param out  Receives the program; only valid if no errors are returned.
 * @return Error messages formatted as `file:line: message`; empty on success.
 */
std::vector<std::string> assemble(const std::string &path, Program &out);
