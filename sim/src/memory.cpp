#include "memory.h"

#include <cstddef>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

#include "isa.h"

namespace Simulator
{
Errors Memory::loadMem(const std::string &path)
{
   std::vector<std::string> lines;

   if (!readLines(path, lines))
   {
      return {"cannot open mem file '" + path + "'"};
   }

   // we added a title as a comment on the first line of each of our mem files, so keep it
   m_title.clear();
   if (!lines.empty())
   {
      std::string first{trim(lines.front())};

      if (!first.empty() && ('#' == first[0] || ';' == first[0]))
      {
         std::size_t start{first.find_first_not_of("#; \t")};
         m_title = (std::string::npos == start) ? "" : first.substr(start);
      }
   }

   bool hasHeader{false};

   for (std::size_t i{0}; i < lines.size(); i++)
   {
      std::size_t lineNum{i + 1};
      std::string body{trim(stripComment(lines[i]))};

      if (body.empty())
      {
         continue;
      }

      std::vector<std::string> tokens{tokenize(body)};
      std::int64_t first{0};
      std::int64_t second{0};

      // per the project requirements, the first line of content holds the address and word sizes.
      if (!hasHeader)
      {
         if (2 != tokens.size() || !parseInteger(tokens[0], 10, first) ||
             !parseInteger(tokens[1], 10, second))
         {
            return {
                formatError(path, lineNum, "expected header '<address bits>, <bits per word>'")};
         }

         if (ADDR_BITS != first || WORD_BITS != second)
         {
            return {formatError(path, lineNum,
                                "this simulator requires header '" + std::to_string(ADDR_BITS) +
                                    ", " + std::to_string(WORD_BITS) + "' (got '" + body + "')")};
         }
         hasHeader = true;
         continue;
      }

      // every other line is an address and the value to store there, both in hex
      if (2 != tokens.size() || !parseInteger(tokens[0], 16, first) ||
          !parseInteger(tokens[1], 16, second))
      {
         return {formatError(path, lineNum,
                             "expected '<hex address>, <hex value>', got '" + body + "'")};
      }

      if (0 > first || MAX_ADDR < first)
      {
         return {formatError(path, lineNum, "address " + tokens[0] + " is outside of memory")};
      }
      if (0 > second || 0xFFFFFFFFll < second)
      {
         return {formatError(path, lineNum, "value " + tokens[1] + " does not fit in 32 bits")};
      }

      write(static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(second));
   }

   if (!hasHeader)
   {
      return {path + ": missing header line for address and word sizes"};
   }

   return {};
}

Errors Memory::writeMem(const std::string &path) const
{
   std::ofstream file{path};

   if (!file.is_open())
   {
      return {"could not write '" + path + "'"};
   }

   file << "# " << (m_title.empty() ? "final memory" : m_title) << '\n'
        << ADDR_BITS << ", " << WORD_BITS << '\n';

   file << std::hex << std::uppercase << std::setfill('0');
   for (const auto &entry : m_storage)
   {
      file << "0x" << std::setw(7) << entry.first << ", 0x" << std::setw(8) << entry.second << '\n';
   }

   return {};
}

} // namespace Simulator
