// memory.h - word-addressable data memory (26-bit address, 32-bit words).
//
// Paged and lazily allocated: a 2^26-word space is 256 MiB, far too much to
// calloc eagerly on a shared machine. Unwritten pages read as zero without
// being allocated, matching the handout ("any address not specified is 0").
// Every store also sets a "written" bit so the .mem.out can list results that
// happen to be zero.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Memory
{
 public:
   static constexpr uint32_t ADDR_BITS = 26;
   static constexpr uint32_t MAX_ADDR = (1u << ADDR_BITS) - 1; // 0x3FFFFFF
   static constexpr uint32_t PAGE_BITS = 12;
   static constexpr uint32_t PAGE_WORDS = 1u << PAGE_BITS;
   static constexpr uint32_t NUM_PAGES = 1u << (ADDR_BITS - PAGE_BITS);

   Memory() : pages_(NUM_PAGES)
   {
   }

   // Caller guarantees addr <= MAX_ADDR.
   uint32_t read(uint32_t addr) const
   {
      const Page *p = pages_[addr >> PAGE_BITS].get();
      return p ? p->data[addr & (PAGE_WORDS - 1)] : 0;
   }
   void write(uint32_t addr, uint32_t value)
   {
      std::unique_ptr<Page> &p = pages_[addr >> PAGE_BITS];
      if (!p)
         p.reset(new Page());
      uint32_t o = addr & (PAGE_WORDS - 1);
      p->data[o] = value;
      p->written[o >> 6] |= uint64_t{1} << (o & 63);
   }

   // Parse a .mem file (see README). Returns "" on success, else an error message.
   std::string loadMemFile(const std::string &path);

   // Write the final contents: every address that was listed in the input .mem
   // or stored to during execution (even if the value is 0), sorted by address.
   // Returns "" or an error message.
   std::string writeMemOut(const std::string &path) const;

 private:
   struct Page
   {
      uint32_t data[PAGE_WORDS] = {};
      uint64_t written[PAGE_WORDS / 64] = {};
   };
   std::vector<std::unique_ptr<Page>> pages_;
   std::string title_; // first-line comment of the input file
};
