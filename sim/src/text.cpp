#include "text.h"

#include <cctype>
#include <cstddef>
#include <fstream>
#include <limits>

namespace Simulator
{
std::string trim(const std::string &str)
{
   std::size_t start{0};
   while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start])))
   {
      start++;
   }

   std::size_t end{str.size()};
   while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1])))
   {
      end--;
   }
   return str.substr(start, end - start);
}

std::string stripComment(const std::string &str)
{
   return str.substr(0, str.find_first_of("#;"));
}

std::string toUpper(std::string str)
{
   for (char &c : str)
   {
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
   }
   return str;
}

std::vector<std::string> tokenize(const std::string &line)
{
   std::vector<std::string> tokens;
   std::string current;

   for (char c : line)
   {
      // anything that is not a delimiter belongs to the current token
      if (',' != c && !std::isspace(static_cast<unsigned char>(c)))
      {
         current += c;
      }
      else if (!current.empty())
      {
         tokens.push_back(current);
         current.clear();
      }
   }

   if (!current.empty())
   {
      tokens.push_back(current);
   }
   return tokens;
}

bool parseInteger(const std::string &str, int base, std::int64_t &result)
{
   std::size_t idx{0};
   bool isNegative{false};

   if (!str.empty() && ('-' == str[0] || '+' == str[0]))
   {
      isNegative = '-' == str[0];
      idx = 1;
   }

   // a 0x prefix is only a prefix when digits follow it
   if (0 != base && 10 != base && 16 != base)
   {
      return false;
   }
   if (10 != base && str.size() - idx > 2 && '0' == str[idx] &&
       ('x' == str[idx + 1] || 'X' == str[idx + 1]))
   {
      base = 16;
      idx += 2;
   }
   else if (0 == base)
   {
      base = 10;
   }

   if (idx >= str.size())
   {
      return false;
   }

   const std::int64_t limit{std::numeric_limits<std::int64_t>::max()};
   std::int64_t value{0};

   for (; idx < str.size(); idx++)
   {
      char c{static_cast<char>(std::tolower(static_cast<unsigned char>(str[idx])))};
      std::int64_t digit{0};

      if ('0' <= c && '9' >= c)
      {
         digit = c - '0';
      }
      else if (16 == base && 'a' <= c && 'f' >= c)
      {
         digit = c - 'a' + 10;
      }
      else
      {
         return false;
      }

      // refuse anything that would overflow instead of wrapping
      if (value > (limit - digit) / base)
      {
         return false;
      }
      value = value * base + digit;
   }

   result = isNegative ? -value : value;
   return true;
}

bool readLines(const std::string &path, std::vector<std::string> &lines)
{
   std::ifstream file{path};

   if (!file.is_open())
   {
      return false;
   }

   lines.clear();
   std::string line;
   while (std::getline(file, line))
   {
      lines.push_back(line);
   }
   return true;
}

std::string formatError(const std::string &path, std::size_t line, const std::string &message)
{
   return path + ":" + std::to_string(line) + ": error: " + message;
}

} // namespace Simulator
