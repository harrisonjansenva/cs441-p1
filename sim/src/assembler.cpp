#include "assembler.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "isa.h"

namespace
{

struct AssemblyItem
{
   uint32_t addr;
   Op op;
   std::vector<std::string> args;
   int line;
};

// Convert string to uppercase
std::string toUpper(std::string str)
{
   for (char &c : str)
   {
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
   }
   return str;
}

// Parse decimal or hex (0x prefix) integer
bool parseInteger(const std::string &str, int64_t &result)
{
   if (str.empty())
   {
      return false;
   }

   size_t idx = 0;
   bool isNegative = false;

   if (str[0] == '-' || str[0] == '+')
   {
      if (str[0] == '-')
      {
         isNegative = true;
      }
      idx = 1;
   }

   if (idx >= str.size())
   {
      return false;
   }

   int base = 10;
   if (str.size() - idx > 2 && str[idx] == '0' && (str[idx + 1] == 'x' || str[idx + 1] == 'X'))
   {
      base = 16;
      idx += 2;
   }

   int64_t val = 0;
   for (; idx < str.size(); idx++)
   {
      char c = std::tolower(static_cast<unsigned char>(str[idx]));
      int digit = 0;
      if (c >= '0' && c <= '9')
      {
         digit = c - '0';
      }
      else if (base == 16 && c >= 'a' && c <= 'f')
      {
         digit = c - 'a' + 10;
      }
      else
      {
         return false;
      }

      val = val * base + digit;
      if (val > (int64_t{1} << 40))
      {
         return false;
      }
   }

   result = isNegative ? -val : val;
   return true;
}

// Split line into tokens by commas and whitespace
std::vector<std::string> tokenize(const std::string &line)
{
   std::vector<std::string> tokens;
   std::string current;
   for (char c : line)
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

int getExpectedOperandCount(Shape s)
{
   switch (s)
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

// Parse a register argument (e.g., "R5")
bool parseRegArg(const std::string &token, uint32_t &regNum, std::string &err)
{
   std::string u = toUpper(token);
   if (u.size() >= 2 && u[0] == 'R')
   {
      int64_t num = 0;
      if (parseInteger(u.substr(1), num))
      {
         if (num >= 0 && num <= 31 && u.find_first_not_of("0123456789", 1) == std::string::npos)
         {
            regNum = static_cast<uint32_t>(num);
            return true;
         }
         err = "register '" + token + "' out of range (R0-R31)";
         return false;
      }
   }
   err = "expected a register, got '" + token + "'";
   return false;
}

// Parse an immediate or label value for 16-bit field
bool parseImm16Arg(const std::string &token, const std::map<std::string, uint32_t> &labels,
                   uint32_t &imm, std::string &err)
{
   int64_t val = 0;
   bool found = parseInteger(token, val);
   if (!found)
   {
      auto it = labels.find(token);
      if (it != labels.end())
      {
         val = it->second;
         found = true;
      }
   }

   if (!found)
   {
      err = "expected an integer immediate, got '" + token + "'";
      return false;
   }

   if (val < -32768 || val > 32767)
   {
      err = "immediate " + token + " does not fit in signed 16 bits";
      return false;
   }

   imm = static_cast<uint32_t>(val) & 0xFFFF;
   return true;
}

// Parse jump or branch target (numeric or label)
bool parseTargetArg(const std::string &token, const std::map<std::string, uint32_t> &labels,
                    uint32_t maxVal, uint32_t maxProgAddr, uint32_t &target, std::string &err)
{
   int64_t val = 0;
   bool found = parseInteger(token, val);
   if (!found)
   {
      auto it = labels.find(token);
      if (it != labels.end())
      {
         val = it->second;
         found = true;
      }
      else
      {
         err = "undefined label '" + token + "'";
         return false;
      }
   }

   if (val < 0 || val > static_cast<int64_t>(maxVal))
   {
      err = "target " + token + " out of range (max " + std::to_string(maxVal) + ")";
      return false;
   }
   if (val > static_cast<int64_t>(maxProgAddr))
   {
      err = "target " + token + " is past the end of the program";
      return false;
   }

   target = static_cast<uint32_t>(val);
   return true;
}

} // namespace

std::vector<std::string> assemble(const std::string &path, Program &out)
{
   std::vector<std::string> errors;
   std::ifstream file(path);
   if (!file.is_open())
   {
      return {"cannot open assembly file '" + path + "'"};
   }

   auto addError = [&](int line, const std::string &msg)
   { errors.push_back(path + ":" + std::to_string(line) + ": error: " + msg); };

   // Pass 1: Read lines, strip comments, collect labels, assign instruction addresses
   std::vector<AssemblyItem> items;
   std::map<std::string, uint32_t> labels;
   uint32_t currentAddr = 0;
   std::string rawLine;
   int lineNum = 0;

   while (std::getline(file, rawLine))
   {
      lineNum++;

      // Strip comments
      size_t commentPos = rawLine.find_first_of("#;");
      std::string line =
          (commentPos == std::string::npos) ? rawLine : rawLine.substr(0, commentPos);

      // Parse labels (e.g. "LOOP:" or "0x20:")
      while (true)
      {
         size_t colon = line.find(':');
         if (colon == std::string::npos)
         {
            break;
         }

         std::string labelName = line.substr(0, colon);
         size_t start = labelName.find_first_not_of(" \t\r");
         if (start == std::string::npos)
         {
            break;
         }
         labelName = labelName.substr(start);

         if (labelName.find_first_of(" \t,\r") != std::string::npos)
         {
            break; // Not a valid single label token
         }

         line = line.substr(colon + 1);

         int64_t origin = 0;
         if (parseInteger(labelName, origin))
         {
            // Numeric label sets instruction origin address
            if (origin < 0 || origin > 0x3FFFFFF)
            {
               addError(lineNum, "origin out of range: " + labelName);
            }
            else
            {
               currentAddr = static_cast<uint32_t>(origin);
            }
         }
         else if (labels.count(labelName) > 0)
         {
            addError(lineNum, "duplicate label '" + labelName + "'");
         }
         else
         {
            labels[labelName] = currentAddr;
         }
      }

      std::vector<std::string> tokens = tokenize(line);
      if (tokens.empty())
      {
         continue;
      }

      Op op = opFromMnemonic(toUpper(tokens[0]));
      if (op == Op::ILLEGAL)
      {
         addError(lineNum, "unknown instruction '" + tokens[0] + "'");
         continue;
      }

      tokens.erase(tokens.begin()); // Remove mnemonic

      int expectedArgs = getExpectedOperandCount(desc(op).shape);
      if (static_cast<int>(tokens.size()) != expectedArgs)
      {
         addError(lineNum, std::string(desc(op).name) + " takes " + std::to_string(expectedArgs) +
                               " operand(s), got " + std::to_string(tokens.size()));
         continue;
      }

      if (currentAddr > 0xFFFFF)
      {
         addError(lineNum, "instruction address too large for the simulator (max 0xFFFFF)");
         continue;
      }

      items.push_back({currentAddr, op, std::move(tokens), lineNum});
      currentAddr++;
   }

   if (items.empty() && errors.empty())
   {
      errors.push_back(path + ": no instructions");
   }
   if (!errors.empty())
   {
      return errors;
   }

   uint32_t maxAddr = 0;
   for (const auto &item : items)
   {
      if (item.addr > maxAddr)
      {
         maxAddr = item.addr;
      }
   }

   out.words.assign(maxAddr + 1, 0);
   out.filled.assign(maxAddr + 1, 0);
   out.srcLine.assign(maxAddr + 1, 0);

   // Pass 2: Encode instructions into 32-bit machine words
   for (const auto &item : items)
   {
      const InstrDesc &d = desc(item.op);
      const auto &args = item.args;
      uint32_t word = static_cast<uint32_t>(d.opcode) << 26;
      std::string err;

      switch (d.shape)
      {
      case Shape::NONE:
         break;

      case Shape::RD_RS1_RS2:
      {
         uint32_t rd = 0, rs1 = 0, rs2 = 0;
         if (!parseRegArg(args[0], rd, err) || !parseRegArg(args[1], rs1, err) ||
             !parseRegArg(args[2], rs2, err))
         {
            addError(item.line, err);
         }
         else
         {
            word |= (rs1 << 21) | (rs2 << 16) | (rd << 11);
         }
         break;
      }

      case Shape::RD_RS1:
      {
         uint32_t rd = 0, rs1 = 0;
         if (!parseRegArg(args[0], rd, err) || !parseRegArg(args[1], rs1, err))
         {
            addError(item.line, err);
         }
         else
         {
            word |= (rs1 << 21) | (rd << 11);
         }
         break;
      }

      case Shape::RD_RS1_RS2_RS3:
      {
         uint32_t rd = 0, rs1 = 0, rs2 = 0, rs3 = 0;
         if (!parseRegArg(args[0], rd, err) || !parseRegArg(args[1], rs1, err) ||
             !parseRegArg(args[2], rs2, err) || !parseRegArg(args[3], rs3, err))
         {
            addError(item.line, err);
         }
         else
         {
            word |= (rs1 << 21) | (rs2 << 16) | (rd << 11) | (rs3 << 6);
         }
         break;
      }

      case Shape::RS1_RS2:
      {
         uint32_t rs1 = 0, rs2 = 0;
         if (!parseRegArg(args[0], rs1, err) || !parseRegArg(args[1], rs2, err))
         {
            addError(item.line, err);
         }
         else
         {
            word |= (rs1 << 21) | (rs2 << 16);
         }
         break;
      }

      case Shape::RT_RS1_IMM:
      {
         uint32_t rt = 0, rs1 = 0, imm = 0;
         if (!parseRegArg(args[0], rt, err) || !parseRegArg(args[1], rs1, err) ||
             !parseImm16Arg(args[2], labels, imm, err))
         {
            addError(item.line, err);
         }
         else
         {
            word |= (rs1 << 21) | (rt << 16) | imm;
         }
         break;
      }

      case Shape::RT_RS1:
      {
         uint32_t rt = 0, rs1 = 0;
         if (!parseRegArg(args[0], rt, err) || !parseRegArg(args[1], rs1, err))
         {
            addError(item.line, err);
         }
         else
         {
            word |= (rs1 << 21) | (rt << 16);
         }
         break;
      }

      case Shape::RT_IMM:
      {
         uint32_t rt = 0, imm = 0;
         if (!parseRegArg(args[0], rt, err) || !parseImm16Arg(args[1], labels, imm, err))
         {
            addError(item.line, err);
         }
         else
         {
            word |= (rt << 16) | imm;
         }
         break;
      }

      case Shape::BR:
      {
         uint32_t rs1 = 0, rs2 = 0, target = 0;
         if (!parseRegArg(args[0], rs1, err) || !parseRegArg(args[1], rs2, err) ||
             !parseTargetArg(args[2], labels, 0xFFFF, maxAddr, target, err))
         {
            addError(item.line, err);
         }
         else
         {
            word |= (rs1 << 21) | (rs2 << 16) | (target & 0xFFFF);
         }
         break;
      }

      case Shape::TGT:
      {
         uint32_t target = 0;
         if (!parseTargetArg(args[0], labels, 0x3FFFFFF, maxAddr, target, err))
         {
            addError(item.line, err);
         }
         else
         {
            word |= (target & 0x3FFFFFF);
         }
         break;
      }

      case Shape::RS1:
      {
         uint32_t rs1 = 0;
         if (!parseRegArg(args[0], rs1, err))
         {
            addError(item.line, err);
         }
         else
         {
            word |= (rs1 << 21);
         }
         break;
      }

      case Shape::RS1_RT:
      {
         uint32_t rs1 = 0, rt = 0;
         if (!parseRegArg(args[0], rs1, err) || !parseRegArg(args[1], rt, err))
         {
            addError(item.line, err);
         }
         else
         {
            word |= (rs1 << 21) | (rt << 16);
         }
         break;
      }
      }

      if (out.filled[item.addr])
      {
         addError(item.line, "address overlaps an earlier instruction");
      }
      out.words[item.addr] = word;
      out.filled[item.addr] = 1;
      out.srcLine[item.addr] = item.line;
   }

   return errors;
}
