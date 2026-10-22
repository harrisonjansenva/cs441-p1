/**
 * @file memory.cpp
 * @brief .mem file parsing and .mem.out writing.
 */
#include "memory.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>

#include "text.h"

namespace
{

/**
 * @brief Parse a hex number with an optional 0x prefix.
 * @param tok Text to parse.
 * @param out Receives the value on success.
 * @return false if the text is empty, too long, or has a non-hex digit.
 */
bool parseHex(const std::string &tok, uint64_t &out)
{
   size_t i = 0;
   if (tok.size() > 2 && tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X'))
      i = 2;
   if (i >= tok.size() || tok.size() - i > 16)
      return false;
   uint64_t v = 0;
   for (; i < tok.size(); ++i)
   {
      int c = std::tolower(static_cast<unsigned char>(tok[i]));
      int d;
      if (c >= '0' && c <= '9')
         d = c - '0';
      else if (c >= 'a' && c <= 'f')
         d = c - 'a' + 10;
      else
         return false;
      v = (v << 4) | static_cast<uint64_t>(d);
   }
   out = v;
   return true;
}

} // namespace

std::string Memory::loadMemFile(const std::string &path)
{
   std::ifstream in(path);
   if (!in)
      return "cannot open memory file '" + path + "'";
   std::string line;
   int lineNo = 0;
   bool haveHeader = false;
   bool first = true;
   auto err = [&](const std::string &m) { return path + ":" + std::to_string(lineNo) + ": " + m; };
   while (std::getline(in, line))
   {
      ++lineNo;
      if (first)
      {
         first = false;
         std::string text = trim(line);
         bool isComment = text.empty() || text[0] == '#' || text[0] == ';';
         if (isComment)
         {
            // the first line is the benchmark-name comment
            while (!text.empty() && (text[0] == '#' || text[0] == ';'))
               text = trim(text.substr(1));
            title_ = text;
            continue;
         }
         // a first line that is not a comment is accepted only if it parses as data below
      }
      std::string body = trim(stripComment(line));
      if (body.empty())
         continue;
      std::vector<std::string> tok = splitTokens(body);
      if (!haveHeader)
      {
         // header line: "26, 32"
         uint64_t a, w;
         if (tok.size() != 2 || !(std::all_of(tok[0].begin(), tok[0].end(), ::isdigit)) ||
             !(std::all_of(tok[1].begin(), tok[1].end(), ::isdigit)))
            return err("expected header '<address bits>, <bits per word>'");
         a = std::stoull(tok[0]);
         w = std::stoull(tok[1]);
         if (a != ADDR_BITS || w != 32)
            return err("this simulator requires header '26, 32' (got '" + body + "')");
         haveHeader = true;
         continue;
      }
      uint64_t addr, val;
      if (tok.size() != 2 || !parseHex(tok[0], addr) || !parseHex(tok[1], val))
         return err("expected '<hex address>, <hex value>', got '" + body + "'");
      if (addr > MAX_ADDR)
         return err("address " + tok[0] + " exceeds 0x3FFFFFF");
      if (val > 0xFFFFFFFFull)
         return err("value " + tok[1] + " does not fit in 32 bits");
      write(static_cast<uint32_t>(addr), static_cast<uint32_t>(val));
   }
   if (!haveHeader)
      return path + ": missing '26, 32' header line";
   return "";
}

std::string Memory::writeMemOut(const std::string &path) const
{
   // collect written addresses in ascending order: pages in order, bits in order
   std::vector<uint32_t> addrs;
   for (uint32_t pg = 0; pg < NUM_PAGES; ++pg)
   {
      const Page *p = pages_[pg].get();
      if (!p)
         continue;
      for (uint32_t i = 0; i < PAGE_WORDS; ++i)
         if (p->written[i >> 6] >> (i & 63) & 1)
            addrs.push_back((pg << PAGE_BITS) | i);
   }

   FILE *f = std::fopen(path.c_str(), "w");
   if (!f)
      return "cannot write '" + path + "'";
   std::fprintf(f, "# %s\n26, 32\n", title_.empty() ? "final memory" : title_.c_str());
   for (uint32_t a : addrs)
      std::fprintf(f, "0x%07X, 0x%08X\n", a, read(a));
   std::fclose(f);
   return "";
}
