#include "assembler.h"

#include <cctype>
#include <fstream>
#include <map>
#include <optional>

#include "isa.h"

namespace
{

struct Item
{
   uint32_t addr;
   Op op;
   std::vector<std::string> args;
   int line;
};

std::string upper(std::string s)
{
   for (char &c : s)
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
   return s;
}

// Decimal or 0x-hex integer with optional sign.
std::optional<int64_t> parseInt(const std::string &t)
{
   if (t.empty())
      return std::nullopt;
   size_t i = 0;
   bool neg = false;
   if (t[0] == '-' || t[0] == '+')
      neg = t[0] == '-', i = 1;
   if (i >= t.size())
      return std::nullopt;
   int base = 10;
   if (t.size() - i > 2 && t[i] == '0' && (t[i + 1] == 'x' || t[i + 1] == 'X'))
      base = 16, i += 2;
   int64_t v = 0;
   for (; i < t.size(); ++i)
   {
      int c = std::tolower(static_cast<unsigned char>(t[i])), d;
      if (c >= '0' && c <= '9')
         d = c - '0';
      else if (base == 16 && c >= 'a' && c <= 'f')
         d = c - 'a' + 10;
      else
         return std::nullopt;
      v = v * base + d;
      if (v > (int64_t{1} << 40))
         return std::nullopt;
   }
   return neg ? -v : v;
}

std::vector<std::string> tokens(const std::string &s)
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

int operandCount(Shape s)
{
   switch (s)
   {
   case Shape::NONE:
      return 0;
   case Shape::RD:
   case Shape::TGT:
   case Shape::RS1:
      return 1;
   case Shape::RD_RS1:
   case Shape::RS1_RS2:
   case Shape::RT_RS1:
   case Shape::RT_IMM:
   case Shape::RS1_RT:
      return 2;
   case Shape::RD_RS1_RS2:
   case Shape::RT_RS1_IMM:
   case Shape::BR:
      return 3;
   case Shape::RD_RS1_RS2_RS3:
      return 4;
   }
   return 0;
}

} // namespace

std::vector<std::string> assemble(const std::string &path, Program &out)
{
   std::vector<std::string> errors;
   std::ifstream in(path);
   if (!in)
      return {"cannot open assembly file '" + path + "'"};

   auto fail = [&](int line, const std::string &m)
   { errors.push_back(path + ":" + std::to_string(line) + ": error: " + m); };

   // ---- Pass 1: split lines, collect labels / origins, assign addresses ----
   std::vector<Item> items;
   std::map<std::string, uint32_t> labels;
   uint32_t addr = 0;
   std::string raw;
   int lineNo = 0;
   while (std::getline(in, raw))
   {
      ++lineNo;
      size_t c = raw.find_first_of("#;");
      std::string s = c == std::string::npos ? raw : raw.substr(0, c);
      // peel off leading "label:" prefixes
      for (;;)
      {
         size_t colon = s.find(':');
         if (colon == std::string::npos)
            break;
         std::string name = s.substr(0, colon);
         size_t b = name.find_first_not_of(" \t\r");
         if (b == std::string::npos)
            break;
         name = name.substr(b);
         if (name.find_first_of(" \t,\r") != std::string::npos)
            break; // ':' is not a label
         s = s.substr(colon + 1);
         if (auto n = parseInt(name))
         { // numeric label = origin
            if (*n < 0 || *n > 0x3FFFFFF)
               fail(lineNo, "origin out of range: " + name);
            else
               addr = static_cast<uint32_t>(*n);
         }
         else if (labels.count(name))
         {
            fail(lineNo, "duplicate label '" + name + "'");
         }
         else
         {
            labels[name] = addr;
         }
      }
      std::vector<std::string> t = tokens(s);
      if (t.empty())
         continue;
      Op op = opFromMnemonic(upper(t[0]));
      if (op == Op::ILLEGAL)
      {
         fail(lineNo, "unknown instruction '" + t[0] + "'");
         continue;
      }
      t.erase(t.begin());
      int want = operandCount(desc(op).shape);
      if (static_cast<int>(t.size()) != want)
      {
         fail(lineNo, std::string(desc(op).name) + " takes " + std::to_string(want) +
                          " operand(s), got " + std::to_string(t.size()));
         continue;
      }
      if (addr > 0xFFFFF)
      {
         fail(lineNo, "instruction address too large for the simulator (max 0xFFFFF)");
         continue;
      }
      items.push_back({addr, op, std::move(t), lineNo});
      ++addr;
   }
   if (items.empty() && errors.empty())
      errors.push_back(path + ": no instructions");
   if (!errors.empty())
      return errors;

   uint32_t maxAddr = 0;
   for (const Item &it : items)
      maxAddr = std::max(maxAddr, it.addr);
   out.words.assign(maxAddr + 1, 0);
   out.filled.assign(maxAddr + 1, 0);
   out.srcLine.assign(maxAddr + 1, 0);

   // ---- Pass 2: encode ----
   for (const Item &it : items)
   {
      const InstrDesc &d = desc(it.op);
      const auto &a = it.args;
      auto reg = [&](const std::string &t) -> uint32_t
      {
         std::string u = upper(t);
         if (u.size() >= 2 && u[0] == 'R')
         {
            auto n = parseInt(u.substr(1));
            if (n && *n >= 0 && *n <= 31 &&
                u.find_first_not_of("0123456789", 1) == std::string::npos)
               return static_cast<uint32_t>(*n);
            if (n)
            {
               fail(it.line, "register '" + t + "' out of range (R0-R31)");
               return 0;
            }
         }
         fail(it.line, "expected a register, got '" + t + "'");
         return 0;
      };
      auto imm16 = [&](const std::string &t) -> uint32_t
      {
         auto n = parseInt(t);
         if (!n)
         {
            if (auto l = labels.find(t); l != labels.end())
               n = l->second; // LDR rt, label
         }
         if (!n)
         {
            fail(it.line, "expected an integer immediate, got '" + t + "'");
            return 0;
         }
         if (*n < -32768 || *n > 32767)
         {
            fail(it.line, "immediate " + t + " does not fit in signed 16 bits");
            return 0;
         }
         return static_cast<uint32_t>(*n) & 0xFFFF;
      };
      // absolute instruction address: label or number, must name a slot inside the program
      auto target = [&](const std::string &t, uint32_t maxVal) -> uint32_t
      {
         int64_t v;
         if (auto n = parseInt(t))
            v = *n;
         else if (auto l = labels.find(t); l != labels.end())
            v = l->second;
         else
         {
            fail(it.line, "undefined label '" + t + "'");
            return 0;
         }
         if (v < 0 || v > static_cast<int64_t>(maxVal))
         {
            fail(it.line, "target " + t + " out of range (max " + std::to_string(maxVal) + ")");
            return 0;
         }
         if (v > static_cast<int64_t>(maxAddr))
         {
            fail(it.line, "target " + t + " is past the end of the program");
            return 0;
         }
         return static_cast<uint32_t>(v);
      };

      uint32_t w = static_cast<uint32_t>(d.opcode) << 26;
      auto R = [&](uint32_t rs1, uint32_t rs2, uint32_t rd, uint32_t sh)
      { w |= rs1 << 21 | rs2 << 16 | rd << 11 | sh << 6; };
      auto I = [&](uint32_t rs1, uint32_t rt, uint32_t imm) { w |= rs1 << 21 | rt << 16 | imm; };
      switch (d.shape)
      {
      case Shape::NONE:
         break;
      case Shape::RD_RS1_RS2:
         R(reg(a[1]), reg(a[2]), reg(a[0]), 0);
         break;
      case Shape::RD_RS1:
         R(reg(a[1]), 0, reg(a[0]), 0);
         break;
      case Shape::RD:
         R(0, 0, reg(a[0]), 0);
         break;
      case Shape::RD_RS1_RS2_RS3:
         R(reg(a[1]), reg(a[2]), reg(a[0]), reg(a[3]));
         break;
      case Shape::RS1_RS2:
         R(reg(a[0]), reg(a[1]), 0, 0);
         break;
      case Shape::RT_RS1_IMM:
         I(reg(a[1]), reg(a[0]), imm16(a[2]));
         break;
      case Shape::RT_RS1:
         I(reg(a[1]), reg(a[0]), 0);
         break;
      case Shape::RT_IMM:
         I(0, reg(a[0]), imm16(a[1]));
         break;
      case Shape::BR:
         I(reg(a[0]), reg(a[1]), target(a[2], 0xFFFF));
         break;
      case Shape::TGT:
         w |= target(a[0], 0x3FFFFFF);
         break;
      case Shape::RS1:
         I(reg(a[0]), 0, 0);
         break;
      case Shape::RS1_RT:
         I(reg(a[0]), reg(a[1]), 0);
         break;
      }
      if (out.filled[it.addr])
         fail(it.line, "address overlaps an earlier instruction");
      out.words[it.addr] = w;
      out.filled[it.addr] = 1;
      out.srcLine[it.addr] = it.line;
   }
   return errors;
}
