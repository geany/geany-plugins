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

#pragma once

#include <regex>
#include <string>
#include <vector>

#define FOUNTAIN_WHITESPACE " \t\n\r\f\v"

std::string &ltrim_inplace(std::string &s, char const *t = FOUNTAIN_WHITESPACE);
std::string &rtrim_inplace(std::string &s, char const *t = FOUNTAIN_WHITESPACE);
std::string &trim_inplace(std::string &s, char const *t = FOUNTAIN_WHITESPACE);

std::string &replace_all_inplace(std::string &subject,
                                 const std::string &search,
                                 const std::string &replace);

std::string ws_ltrim(std::string s);
std::string ws_rtrim(std::string s);
std::string ws_trim(std::string s);

std::string replace_all(std::string subject, const std::string &search,
                        const std::string &replace);

bool begins_with(std::string const &input, std::string const &match);

std::vector<std::string> split_string(std::string const &str,
                                      std::string const &delimiter);

std::vector<std::string> split_lines(std::string const &s);

std::string &to_upper_inplace(std::string &s);
std::string &to_lower_inplace(std::string &s);

std::string to_upper(std::string s);
std::string to_lower(std::string s);

bool is_upper(std::string const &s);

std::string cstr_assign(char *input);
std::string file_get_contents(std::string const &filename);

bool file_set_contents(std::string const &filename,
                       std::string const &contents);

void print_regex_error(std::regex_error &e);
