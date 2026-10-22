/**
 * @file assembler.cpp
 * @brief Two-pass assembler implementation.
 */
#include "assembler.h"

#include <cctype>
#include <fstream>
#include <map>
#include <optional>

#include "isa.h"
#include "text.h"

namespace
{

constexpr uint32_t MAX_ORIGIN = 0x3FFFFFF;      ///< largest address a numeric label may set
constexpr uint32_t MAX_INSTR_ADDR = 0xFFFFF;    ///< largest address the simulator assembles to
constexpr uint32_t MAX_BRANCH_TARGET = 0xFFFF;  ///< branch targets live in imm16
constexpr uint32_t MAX_JUMP_TARGET = 0x3FFFFFF; ///< jump targets are 26 bits
constexpr int64_t MAX_PARSED_VALUE = int64_t{1} << 40; ///< stops parseInt from overflowing

/// One parsed instruction line, waiting for pass 2 to encode it.
struct Item
{
   uint32_t addr;                 ///< instruction address
   Op op;                         ///< instruction
   std::vector<std::string> args; ///< operand tokens
   int line;                      ///< source line, for error messages
};

/// Upper-case copy of a string.
std::string upper(std::string text)
{
   for (char &ch : text)
      ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
   return text;
}

/// Parse a decimal or 0x-hex integer with an optional sign.
std::optional<int64_t> parseInt(const std::string &text)
{
   if (text.empty())
      return std::nullopt;

   size_t pos = 0;
   bool negative = false;
   if (text[0] == '-' || text[0] == '+')
   {
      negative = text[0] == '-';
      pos = 1;
   }
   if (pos >= text.size())
      return std::nullopt;

   int base = 10;
   if (text.size() - pos > 2 && text[pos] == '0' && (text[pos + 1] == 'x' || text[pos + 1] == 'X'))
   {
      base = 16;
      pos += 2;
   }

   int64_t value = 0;
   for (; pos < text.size(); ++pos)
   {
      int ch = std::tolower(static_cast<unsigned char>(text[pos]));
      int digit;
      if (ch >= '0' && ch <= '9')
         digit = ch - '0';
      else if (base == 16 && ch >= 'a' && ch <= 'f')
         digit = ch - 'a' + 10;
      else
         return std::nullopt;

      value = value * base + digit;
      if (value > MAX_PARSED_VALUE)
         return std::nullopt;
   }
   return negative ? -value : value;
}

/// Number of assembly operands an instruction of this shape takes.
int operandCount(Shape shape)
{
   switch (shape)
   {
   case Shape::NONE:
      return 0;
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

   auto fail = [&](int line, const std::string &message)
   { errors.push_back(path + ":" + std::to_string(line) + ": error: " + message); };

   // Pass 1: split lines, collect labels and origins, assign addresses
   std::vector<Item> items;
   std::map<std::string, uint32_t> labels;
   uint32_t addr = 0;
   std::string rawLine;
   int lineNo = 0;
   while (std::getline(in, rawLine))
   {
      ++lineNo;
      std::string text = stripComment(rawLine);

      // peel off leading "label:" prefixes
      for (;;)
      {
         size_t colon = text.find(':');
         if (colon == std::string::npos)
            break;

         std::string name = text.substr(0, colon);
         size_t nameStart = name.find_first_not_of(" \t\r");
         if (nameStart == std::string::npos)
            break;
         name = name.substr(nameStart);
         if (name.find_first_of(" \t,\r") != std::string::npos)
            break; // the ':' is not part of a label

         text = text.substr(colon + 1);
         if (auto origin = parseInt(name))
         {
            // a numeric label sets the address of the next instruction
            if (*origin < 0 || *origin > MAX_ORIGIN)
               fail(lineNo, "origin out of range: " + name);
            else
               addr = static_cast<uint32_t>(*origin);
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

      std::vector<std::string> operands = splitTokens(text);
      if (operands.empty())
         continue;

      Op op = opFromMnemonic(upper(operands[0]));
      if (op == Op::ILLEGAL)
      {
         fail(lineNo, "unknown instruction '" + operands[0] + "'");
         continue;
      }
      operands.erase(operands.begin());

      int expected = operandCount(desc(op).shape);
      if (static_cast<int>(operands.size()) != expected)
      {
         fail(lineNo, std::string(desc(op).name) + " takes " + std::to_string(expected) +
                          " operand(s), got " + std::to_string(operands.size()));
         continue;
      }
      if (addr > MAX_INSTR_ADDR)
      {
         fail(lineNo, "instruction address too large for the simulator (max 0xFFFFF)");
         continue;
      }
      items.push_back({addr, op, std::move(operands), lineNo});
      ++addr;
   }
   if (items.empty() && errors.empty())
      errors.push_back(path + ": no instructions");
   if (!errors.empty())
      return errors;

   uint32_t maxAddr = 0;
   for (const Item &item : items)
      maxAddr = std::max(maxAddr, item.addr);
   out.words.assign(maxAddr + 1, 0);
   out.filled.assign(maxAddr + 1, 0);
   out.srcLine.assign(maxAddr + 1, 0);

   // Pass 2: encode
   for (const Item &item : items)
   {
      const InstrDesc &info = desc(item.op);
      const auto &args = item.args;

      auto reg = [&](const std::string &token) -> uint32_t
      {
         std::string name = upper(token);
         if (name.size() >= 2 && name[0] == 'R')
         {
            auto number = parseInt(name.substr(1));
            bool allDigits = name.find_first_not_of("0123456789", 1) == std::string::npos;
            if (number && *number >= 0 && *number <= 31 && allDigits)
               return static_cast<uint32_t>(*number);
            if (number)
            {
               fail(item.line, "register '" + token + "' out of range (R0-R31)");
               return 0;
            }
         }
         fail(item.line, "expected a register, got '" + token + "'");
         return 0;
      };

      auto imm16 = [&](const std::string &token) -> uint32_t
      {
         auto number = parseInt(token);
         if (!number)
         {
            // LDR rt, label
            if (auto label = labels.find(token); label != labels.end())
               number = label->second;
         }
         if (!number)
         {
            fail(item.line, "expected an integer immediate, got '" + token + "'");
            return 0;
         }
         if (*number < -32768 || *number > 32767)
         {
            fail(item.line, "immediate " + token + " does not fit in signed 16 bits");
            return 0;
         }
         return static_cast<uint32_t>(*number) & 0xFFFF;
      };

      // absolute instruction address: a label or number that names a slot inside the program
      auto target = [&](const std::string &token, uint32_t maxValue) -> uint32_t
      {
         int64_t value;
         if (auto number = parseInt(token))
         {
            value = *number;
         }
         else if (auto label = labels.find(token); label != labels.end())
         {
            value = label->second;
         }
         else
         {
            fail(item.line, "undefined label '" + token + "'");
            return 0;
         }
         if (value < 0 || value > static_cast<int64_t>(maxValue))
         {
            fail(item.line,
                 "target " + token + " out of range (max " + std::to_string(maxValue) + ")");
            return 0;
         }
         if (value > static_cast<int64_t>(maxAddr))
         {
            fail(item.line, "target " + token + " is past the end of the program");
            return 0;
         }
         return static_cast<uint32_t>(value);
      };

      uint32_t opcode = info.opcode;
      uint32_t word = makeJ(opcode, 0);
      switch (info.shape)
      {
      case Shape::NONE:
         break;
      case Shape::RD_RS1_RS2:
         word = makeR(opcode, reg(args[1]), reg(args[2]), reg(args[0]));
         break;
      case Shape::RD_RS1:
         word = makeR(opcode, reg(args[1]), 0, reg(args[0]));
         break;
      case Shape::RD_RS1_RS2_RS3:
         word = makeR(opcode, reg(args[1]), reg(args[2]), reg(args[0]), reg(args[3]));
         break;
      case Shape::RS1_RS2:
         word = makeR(opcode, reg(args[0]), reg(args[1]), 0);
         break;
      case Shape::RT_RS1_IMM:
         word = makeI(opcode, reg(args[1]), reg(args[0]), imm16(args[2]));
         break;
      case Shape::RT_RS1:
         word = makeI(opcode, reg(args[1]), reg(args[0]), 0);
         break;
      case Shape::RT_IMM:
         word = makeI(opcode, 0, reg(args[0]), imm16(args[1]));
         break;
      case Shape::BR:
         word = makeI(opcode, reg(args[0]), reg(args[1]), target(args[2], MAX_BRANCH_TARGET));
         break;
      case Shape::TGT:
         word = makeJ(opcode, target(args[0], MAX_JUMP_TARGET));
         break;
      case Shape::RS1:
         word = makeI(opcode, reg(args[0]), 0, 0);
         break;
      case Shape::RS1_RT:
         word = makeI(opcode, reg(args[0]), reg(args[1]), 0);
         break;
      }

      if (out.filled[item.addr])
         fail(item.line, "address overlaps an earlier instruction");
      out.words[item.addr] = word;
      out.filled[item.addr] = 1;
      out.srcLine[item.addr] = item.line;
   }
   return errors;
}
