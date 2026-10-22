/**
 * @file memory.h
 * @brief Word-addressable data memory (26-bit address, 32-bit words).
 */
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Data memory.
 *
 * Paged and lazily allocated: a 2^26-word space is 256 MiB, far too much to
 * allocate eagerly on a shared machine. Unwritten pages read as zero without
 * being allocated, matching the handout ("any address not specified is 0").
 * Every store also sets a "written" bit so the .mem.out can list results that
 * happen to be zero.
 */
class Memory
{
 public:
   static constexpr uint32_t ADDR_BITS = 26;                   ///< width of a data address
   static constexpr uint32_t MAX_ADDR = (1u << ADDR_BITS) - 1; ///< highest address, 0x3FFFFFF
   static constexpr uint32_t PAGE_BITS = 12;                   ///< log2 of words per page
   static constexpr uint32_t PAGE_WORDS = 1u << PAGE_BITS;     ///< words per page
   static constexpr uint32_t NUM_PAGES = 1u << (ADDR_BITS - PAGE_BITS); ///< pages in the space

   Memory() : pages_(NUM_PAGES)
   {
   }

   /**
    * @brief Read one word.
    * @param addr Word address; the caller guarantees addr <= MAX_ADDR.
    * @return The stored value, or 0 if the address was never written.
    */
   uint32_t read(uint32_t addr) const
   {
      const Page *p = pages_[addr >> PAGE_BITS].get();
      return p ? p->data[addr & (PAGE_WORDS - 1)] : 0;
   }
   /**
    * @brief Write one word and mark its address as written.
    * @param addr  Word address; the caller guarantees addr <= MAX_ADDR.
    * @param value Value to store.
    */
   void write(uint32_t addr, uint32_t value)
   {
      std::unique_ptr<Page> &p = pages_[addr >> PAGE_BITS];
      if (!p)
         p = std::make_unique<Page>();
      uint32_t o = addr & (PAGE_WORDS - 1);
      p->data[o] = value;
      p->written[o >> 6] |= uint64_t{1} << (o & 63);
   }

   /**
    * @brief Load the initial contents from a .mem file (format in the README).
    * @param path Memory file.
    * @return Empty string on success, otherwise an error message.
    */
   std::string loadMemFile(const std::string &path);

   /**
    * @brief Write the final contents, sorted by address.
    *
    * Includes every address listed in the input .mem or stored to during
    * execution, even if its value is 0.
    *
    * @param path Output file, e.g. `benchmark1.mem.out`.
    * @return Empty string on success, otherwise an error message.
    */
   std::string writeMemOut(const std::string &path) const;

 private:
   /** @brief One allocated page of memory. */
   struct Page
   {
      uint32_t data[PAGE_WORDS] = {};         ///< word contents
      uint64_t written[PAGE_WORDS / 64] = {}; ///< bitmap of words ever written
   };
   std::vector<std::unique_ptr<Page>> pages_; ///< null until a page is first written
   std::string title_;                        ///< first-line comment of the input file
};
