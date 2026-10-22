#pragma once

#include <cstdint>
#include <map>
#include <string>

// Word-addressable memory (26-bit address space).
// Uses a map to store addresses that have been loaded or written to,
// so uninitialized memory reads as 0 without allocating 256MB.
class Memory
{
 public:
   static constexpr uint32_t ADDR_BITS = 26;
   static constexpr uint32_t MAX_ADDR = (1u << ADDR_BITS) - 1; // 0x3FFFFFF

   Memory() = default;

   // Reads a word from memory. Returns 0 if address was not written.
   uint32_t read(uint32_t addr) const
   {
      auto it = storage_.find(addr);
      if (it != storage_.end())
      {
         return it->second;
      }
      return 0;
   }

   // Writes a word to memory and records the address.
   void write(uint32_t addr, uint32_t value)
   {
      storage_[addr] = value;
   }

   // Parse a .mem file. Returns empty string on success, else an error message.
   std::string loadMemFile(const std::string &path);

   // Writes final memory contents sorted by address to .mem.out.
   std::string writeMemOut(const std::string &path) const;

 private:
   std::map<uint32_t, uint32_t> storage_;
   std::string title_;
};
