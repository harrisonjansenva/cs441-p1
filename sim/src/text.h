/**
 * @file text.h
 * @brief Small string helpers shared by the assembler and the .mem loader.
 */
#pragma once
#include <cctype>
#include <string>
#include <vector>

/** @brief Copy of a string without leading or trailing whitespace. */
inline std::string trim(const std::string &text)
{
   size_t begin = 0;
   size_t end = text.size();
   while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])))
      ++begin;
   while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
      --end;
   return text.substr(begin, end - begin);
}

/** @brief Drop everything from the first `#` or `;` onward. */
inline std::string stripComment(const std::string &line)
{
   size_t start = line.find_first_of("#;");
   return start == std::string::npos ? line : line.substr(0, start);
}

/** @brief Split a line into tokens at whitespace and commas. */
inline std::vector<std::string> splitTokens(const std::string &line)
{
   std::vector<std::string> tokens;
   std::string current;
   for (char ch : line)
   {
      if (ch == ',' || std::isspace(static_cast<unsigned char>(ch)))
      {
         if (!current.empty())
         {
            tokens.push_back(current);
            current.clear();
         }
      }
      else
      {
         current += ch;
      }
   }
   if (!current.empty())
      tokens.push_back(current);
   return tokens;
}
