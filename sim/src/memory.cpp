#include "memory.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>

namespace
{

std::string trim(const std::string &s)
{
   size_t b = 0, e = s.size();
   while (b < e && std::isspace(static_cast<unsigned char>(s[b])))
      ++b;
   while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
      --e;
   return s.substr(b, e - b);
}

std::string stripComment(const std::string &s)
{
   size_t p = s.find_first_of("#;");
   return p == std::string::npos ? s : s.substr(0, p);
}

// Parse a hex number with optional 0x prefix. Returns false on any bad digit.
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

std::vector<std::string> splitTokens(const std::string &s)
{
   std::vector<std::string> t;
   std::string cur;
   for (char c : s)
   {
      if (c == ',' || std::isspace(static_cast<unsigned char>(c)))
      {
         if (!cur.empty())
            t.push_back(cur), cur.clear();
      }
      else
         cur += c;
   }
   if (!cur.empty())
      t.push_back(cur);
   return t;
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
      { // first line is the benchmark-name comment
         first = false;
         std::string t = trim(line);
         while (!t.empty() && (t[0] == '#' || t[0] == ';'))
            t = trim(t.substr(1));
         title_ = t;
         // A first line that is not a comment is accepted only if it parses as data below.
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
         continue;
      std::vector<std::string> tok = splitTokens(body);
      if (!haveHeader)
      { // "26, 32"
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
   std::vector<uint32_t> addrs; // ascending: pages in order, bits in order
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
