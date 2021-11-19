/*
 * Auxiliary Functions - Fountain Screenplay Processor
 * Copyright 2021 xiota
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <fstream>
#include <iostream>

#include "auxiliary.h"

std::string &ltrim_inplace(std::string &s, char const *t) {
  s.erase(0, s.find_first_not_of(t));
  return s;
}

std::string &rtrim_inplace(std::string &s, char const *t) {
  s.erase(s.find_last_not_of(t) + 1);
  return s;
}

std::string &trim_inplace(std::string &s, char const *t) {
  return ltrim_inplace(rtrim_inplace(s, t), t);
}

std::string &replace_all_inplace(std::string &subject,
                                 const std::string &search,
                                 const std::string &replace) {
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
  }
  return subject;
}

std::string ws_ltrim(std::string s) {
  return ltrim_inplace(s, FOUNTAIN_WHITESPACE);
}

std::string ws_rtrim(std::string s) {
  return rtrim_inplace(s, FOUNTAIN_WHITESPACE);
}

std::string ws_trim(std::string s) {
  return trim_inplace(s, FOUNTAIN_WHITESPACE);
}

std::string replace_all(std::string subject, const std::string &search,
                        const std::string &replace) {
  return replace_all_inplace(subject, search, replace);
}

bool begins_with(std::string const &input, std::string const &match) {
  return !strncmp(input.c_str(), match.c_str(), match.length());
}

std::vector<std::string> split_string(std::string const &str,
                                      std::string const &delimiter) {
  std::vector<std::string> strings;

  std::string::size_type pos = 0;
  std::string::size_type prev = 0;
  while ((pos = str.find(delimiter, prev)) != std::string::npos) {
    strings.push_back(str.substr(prev, pos - prev));
    prev = pos + 1;
  }

  // to get the last substring
  // (or entire string if delimiter is not found)
  strings.push_back(str.substr(prev));

  return strings;
}

std::vector<std::string> split_lines(std::string const &s) {
  return split_string(s, "\n");
}

std::string &to_upper_inplace(std::string &s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return s;
}

std::string &to_lower_inplace(std::string &s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

std::string to_upper(std::string s) { return to_upper_inplace(s); }
std::string to_lower(std::string s) { return to_lower_inplace(s); }

bool is_upper(std::string const &s) {
  return std::all_of(s.begin(), s.end(),
                     [](unsigned char c) { return !std::islower(c); });
}

std::string cstr_assign(char *input) {
  if (input) {
    std::string output{input};
    free(input);
    return output;
  }
  return {};
}

std::string file_get_contents(std::string const &filename) {
  std::string content;
  try {
    std::ifstream instream(filename.c_str(), std::ios::in);
    content.assign((std::istreambuf_iterator<char>(instream)),
                   (std::istreambuf_iterator<char>()));
    instream.close();
  } catch (...) {
    // do nothing;
  }
  return content;
}

bool file_set_contents(std::string const &filename,
                       std::string const &contents) {
  try {
    std::ofstream outstream(filename.c_str(), std::ios::out);
    copy(contents.begin(), contents.end(),
         std::ostream_iterator<char>(outstream));
    outstream.close();
    return true;
  } catch (...) {
    return false;
  }
}

void print_regex_error(std::regex_error &e) {
  switch (e.code()) {
    case std::regex_constants::error_collate:
      fprintf(
          stderr,
          "%d: The expression contained an invalid collating element name.\n",
          e.code());
      break;
    case std::regex_constants::error_ctype:
      fprintf(stderr,
              "%d: The expression contained an invalid character class name.\n",
              e.code());
      break;
    case std::regex_constants::error_escape:
      fprintf(stderr,
              "%d: The expression contained an invalid escaped character, "
              "or a trailing escape.\n",
              e.code());
      break;
    case std::regex_constants::error_backref:
      fprintf(stderr,
              "%d: The expression contained an invalid back reference.\n",
              e.code());
      break;
    case std::regex_constants::error_brack:
      fprintf(stderr,
              "%d: The expression contained mismatched brackets ([ and ]).\n",
              e.code());
      break;
    case std::regex_constants::error_paren:
      fprintf(
          stderr,
          "%d: The expression contained mismatched parentheses (( and )).\n",
          e.code());
      break;
    case std::regex_constants::error_brace:
      fprintf(stderr,
              "%d: The expression contained mismatched braces ({ and }).\n",
              e.code());
      break;
    case std::regex_constants::error_badbrace:
      fprintf(stderr,
              "%d: The expression contained an invalid range between braces "
              "({ and }).\n",
              e.code());
      break;
    case std::regex_constants::error_range:
      fprintf(stderr,
              "%d: The expression contained an invalid character range.\n",
              e.code());
      break;
    case std::regex_constants::error_space:
      fprintf(stderr,
              "%d: There was insufficient memory to convert the expression "
              "into a finite state machine.\n",
              e.code());
      break;
    case std::regex_constants::error_badrepeat:
      fprintf(
          stderr,
          "%d: The expression contained a repeat specifier (one of *?+{) that "
          "was not preceded by a valid regular expression.\n",
          e.code());
      break;
    case std::regex_constants::error_complexity:
      fprintf(stderr,
              "%d: The complexity of an attempted match against a regular "
              "expression exceeded a pre-set level.\n",
              e.code());
      break;
    case std::regex_constants::error_stack:
      fprintf(
          stderr,
          "%d: There was insufficient memory to determine whether the regular "
          "expression could match the specified character sequence.\n",
          e.code());
      break;
    default:
      fprintf(stderr, "%d: unknown regex error\n", e.code());
  }
}
