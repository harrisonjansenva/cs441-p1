#include "assembler.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "isa.h"

namespace Simulator
{
namespace
{
using Labels = std::map<std::string, std::uint32_t>;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief a representation of an assembly instruction
////////////////////////////////////////////////////////////////////////////////////////////////////
struct AssemblyItem
{
   std::uint32_t addr;
   Op op;
   std::vector<std::string> args;
   std::size_t line;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Parse a register argument (e.g., "R5")
////////////////////////////////////////////////////////////////////////////////////////////////////
bool parseRegArg(const std::string &token, std::uint32_t &regNum, std::string &err)
{
   std::string upper{toUpper(token)};
   std::int64_t num{0};

   if (2 <= upper.size() && 'R' == upper[0] &&
       std::string::npos == upper.find_first_not_of("0123456789", 1) &&
       parseInteger(upper.substr(1), 10, num))
   {
      if (0 <= num && 31 >= num)
      {
         regNum = static_cast<std::uint32_t>(num);
         return true;
      }
      err = "register '" + token + "' out of range (R0-R31)";
      return false;
   }

   err = "expected a register, got '" + token + "'";
   return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief look up a number, which is either written out or is the address of a label.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool parseNumberOrLabel(const std::string &token, const Labels &labels, std::int64_t &value)
{
   if (parseInteger(token, 0, value))
   {
      return true;
   }

   auto it{labels.find(token)};
   if (labels.end() != it)
   {
      value = it->second;
      return true;
   }
   return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Parse an immediate or label value for 16-bit field
////////////////////////////////////////////////////////////////////////////////////////////////////
bool parseImm16Arg(const std::string &token, const Labels &labels, std::uint32_t &imm,
                   std::string &err)
{
   std::int64_t val{0};

   if (!parseNumberOrLabel(token, labels, val))
   {
      err = "expected an integer immediate, got '" + token + "'";
      return false;
   }

   if (-32768 > val || 32767 < val)
   {
      err = "immediate " + token + " does not fit in signed 16 bits";
      return false;
   }

   imm = static_cast<std::uint32_t>(val) & 0xFFFF;
   return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Parse jump or branch target (numeric or label)
/// @param maxVal: the largest target the instruction can encode
/// @param maxProgAddr: the address of the last instruction in the program
////////////////////////////////////////////////////////////////////////////////////////////////////
bool parseTargetArg(const std::string &token, const Labels &labels, std::uint32_t maxVal,
                    std::uint32_t maxProgAddr, std::uint32_t &target, std::string &err)
{
   std::int64_t val{0};

   if (!parseNumberOrLabel(token, labels, val))
   {
      err = "undefined label '" + token + "'";
      return false;
   }

   if (0 > val || maxVal < val)
   {
      err = "target " + token + " out of range (max " + std::to_string(maxVal) + ")";
      return false;
   }
   if (maxProgAddr < val)
   {
      err = "target " + token + " is past the end of the program";
      return false;
   }

   target = static_cast<std::uint32_t>(val);
   return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief encode one operand into its field of the instruction word.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool encodeOperand(Operand operand, const std::string &token, const Labels &labels,
                   std::uint32_t maxProgAddr, std::uint32_t &word, std::string &err)
{
   std::uint32_t field{0};
   bool ok{false};

   switch (operand)
   {
   case Operand::Imm16:
      ok = parseImm16Arg(token, labels, field, err);
      break;
   case Operand::Target16:
      ok = parseTargetArg(token, labels, MAX_BRANCH_TARGET, maxProgAddr, field, err);
      break;
   case Operand::Target26:
      ok = parseTargetArg(token, labels, MAX_ADDR, maxProgAddr, field, err);
      break;
   default:
      ok = parseRegArg(token, field, err);
      break;
   }

   if (ok)
   {
      word |= field << operandShift(operand);
   }
   return ok;
}

} // anonymous namespace

Errors assemble(const std::string &path, Program &out)
{
   Errors errors;
   std::vector<std::string> lines;

   if (!readLines(path, lines))
   {
      return {"cannot open assembly file '" + path + "'"};
   }

   auto addError = [&](std::size_t line, const std::string &msg)
   { errors.push_back(formatError(path, line, msg)); };

   // Pass 1: strip comments, collect labels, assign instruction addresses
   std::vector<AssemblyItem> items;
   Labels labels;
   std::uint32_t currentAddr{0};

   for (std::size_t i{0}; i < lines.size(); i++)
   {
      std::size_t lineNum{i + 1};
      std::string line{stripComment(lines[i])};

      // Parse labels (e.g. "LOOP:" or "0x20:")
      while (true)
      {
         std::size_t colon{line.find(':')};
         if (std::string::npos == colon)
         {
            break;
         }

         std::string labelName{trim(line.substr(0, colon))};
         if (labelName.empty() || std::string::npos != labelName.find_first_of(" \t,"))
         {
            break; // Not a valid single label token
         }

         line = line.substr(colon + 1);

         std::int64_t origin{0};
         if (parseInteger(labelName, 0, origin))
         {
            // Numeric label sets instruction origin address
            if (0 > origin || MAX_ADDR < origin)
            {
               addError(lineNum, "origin out of range: " + labelName);
            }
            else
            {
               currentAddr = static_cast<std::uint32_t>(origin);
            }
         }
         else if (0 < labels.count(labelName))
         {
            addError(lineNum, "duplicate label '" + labelName + "'");
         }
         else
         {
            labels[labelName] = currentAddr;
         }
      }

      std::vector<std::string> tokens{tokenize(line)};
      if (tokens.empty())
      {
         continue;
      }

      Op op{fromMnemonic(toUpper(tokens[0]))};
      if (Op::ILLEGAL == op)
      {
         addError(lineNum, "unknown instruction '" + tokens[0] + "'");
         continue;
      }

      tokens.erase(tokens.begin()); // Remove mnemonic

      std::size_t expectedArgs{layout(desc(op).shape).count};
      if (expectedArgs != tokens.size())
      {
         addError(lineNum, std::string(desc(op).name) + " takes " + std::to_string(expectedArgs) +
                               " operand(s), got " + std::to_string(tokens.size()));
         continue;
      }

      if (MAX_PROGRAM_ADDR < currentAddr)
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

   std::uint32_t maxAddr{0};
   for (const auto &item : items)
   {
      if (item.addr > maxAddr)
      {
         maxAddr = item.addr;
      }
   }

   out.entries.assign(maxAddr + 1, Program::Entry{});

   // Pass 2: Encode instructions into 32-bit machine words
   for (const auto &item : items)
   {
      const InstrDesc &d{desc(item.op)};
      const ShapeLayout &shape{layout(d.shape)};
      std::uint32_t word{static_cast<std::uint32_t>(d.opcode) << 26};
      std::string err;

      for (std::size_t i{0}; i < shape.count; i++)
      {
         if (!encodeOperand(shape.operands[i], item.args[i], labels, maxAddr, word, err))
         {
            addError(item.line, err);
            break;
         }
      }

      Program::Entry &entry{out.entries[item.addr]};
      if (entry.filled)
      {
         addError(item.line, "address overlaps an earlier instruction");
      }
      entry = {word, item.line, true};
   }

   return errors;
}

} // namespace Simulator
