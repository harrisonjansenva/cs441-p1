#include "memory.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <vector>

namespace
{

// Trim whitespace from start and end of string
std::string trim(const std::string &s)
{
   size_t start = 0;
   while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
   {
      start++;
   }
   size_t end = s.size();
   while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
   {
      end--;
   }
   return s.substr(start, end - start);
}

// Strip comment (# or ;) from line
std::string stripComment(const std::string &s)
{
   size_t pos = s.find_first_of("#;");
   if (pos == std::string::npos)
   {
      return s;
   }
   return s.substr(0, pos);
}

// Parse a hex value (e.g., "0x1234" or "1234")
bool parseHex(const std::string &token, uint64_t &result)
{
   size_t i = 0;
   if (token.size() > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X'))
   {
      i = 2;
   }
   if (i >= token.size() || token.size() - i > 16)
   {
      return false;
   }

   uint64_t val = 0;
   for (; i < token.size(); i++)
   {
      char c = std::tolower(static_cast<unsigned char>(token[i]));
      int digit = 0;
      if (c >= '0' && c <= '9')
      {
         digit = c - '0';
      }
      else if (c >= 'a' && c <= 'f')
      {
         digit = c - 'a' + 10;
      }
      else
      {
         return false;
      }
      val = (val << 4) | static_cast<uint64_t>(digit);
   }
   result = val;
   return true;
}

// Split line by commas and spaces
std::vector<std::string> splitTokens(const std::string &s)
{
   std::vector<std::string> tokens;
   std::string current;
   for (char c : s)
   {
      if (c == ',' || std::isspace(static_cast<unsigned char>(c)))
      {
         if (!current.empty())
         {
            tokens.push_back(current);
            current.clear();
         }
      }
      else
      {
         current += c;
      }
   }
   if (!current.empty())
   {
      tokens.push_back(current);
   }
   return tokens;
}

} // namespace

std::string Memory::loadMemFile(const std::string &path)
{
   std::ifstream file(path);
   if (!file.is_open())
   {
      return "cannot open memory file '" + path + "'";
   }

   std::string line;
   int lineNum = 0;
   bool hasHeader = false;
   bool firstLine = true;

   while (std::getline(file, line))
   {
      lineNum++;

      // First line is the benchmark name comment
      if (firstLine)
      {
         firstLine = false;
         std::string t = trim(line);
         while (!t.empty() && (t[0] == '#' || t[0] == ';'))
         {
            t = trim(t.substr(1));
         }
         title_ = t;

         if (!trim(line).empty() && trim(line)[0] != '#' && trim(line)[0] != ';')
         {
            title_.clear();
         }
         else
         {
            continue;
         }
      }

      std::string body = trim(stripComment(line));
      if (body.empty())
      {
         continue;
      }

      std::vector<std::string> tok = splitTokens(body);

      // Header check: "26, 32"
      if (!hasHeader)
      {
         if (tok.size() != 2 || !std::all_of(tok[0].begin(), tok[0].end(), ::isdigit) ||
             !std::all_of(tok[1].begin(), tok[1].end(), ::isdigit))
         {
            return path + ":" + std::to_string(lineNum) +
                   ": expected header '<address bits>, <bits per word>'";
         }

         uint64_t addrBits = std::stoull(tok[0]);
         uint64_t wordBits = std::stoull(tok[1]);
         if (addrBits != ADDR_BITS || wordBits != 32)
         {
            return path + ":" + std::to_string(lineNum) +
                   ": this simulator requires header '26, 32' (got '" + body + "')";
         }
         hasHeader = true;
         continue;
      }

      // Memory entry: address, value
      uint64_t addr = 0;
      uint64_t val = 0;
      if (tok.size() != 2 || !parseHex(tok[0], addr) || !parseHex(tok[1], val))
      {
         return path + ":" + std::to_string(lineNum) +
                ": expected '<hex address>, <hex value>', got '" + body + "'";
      }

      if (addr > MAX_ADDR)
      {
         return path + ":" + std::to_string(lineNum) + ": address " + tok[0] + " exceeds 0x3FFFFFF";
      }
      if (val > 0xFFFFFFFFull)
      {
         return path + ":" + std::to_string(lineNum) + ": value " + tok[1] +
                " does not fit in 32 bits";
      }

      write(static_cast<uint32_t>(addr), static_cast<uint32_t>(val));
   }

   if (!hasHeader)
   {
      return path + ": missing '26, 32' header line";
   }

   return "";
}

std::string Memory::writeMemOut(const std::string &path) const
{
   FILE *f = std::fopen(path.c_str(), "w");
   if (!f)
   {
      return "cannot write '" + path + "'";
   }

   std::fprintf(f, "# %s\n26, 32\n", title_.empty() ? "final memory" : title_.c_str());

   // Map keys are automatically in ascending address order
   for (const auto &entry : storage_)
   {
      std::fprintf(f, "0x%07X, 0x%08X\n", entry.first, entry.second);
   }

   std::fclose(f);
   return "";
}
