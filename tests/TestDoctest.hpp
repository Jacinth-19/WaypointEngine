// Waypoint Engine — the one place the test suite pulls in doctest.
// SPDX-License-Identifier: Apache-2.0
//
// doctest only forward-declares std::basic_ostream, and MSVC's <string_view>
// defines its operator<< in terms of that type. A CHECK() that compares a
// std::string_view with a string literal therefore fails to compile on MSVC
// ("use of undefined type 'std::basic_ostream<char, std::char_traits<char>>'")
// unless the real definition is visible first. Including <ostream> here means
// every test file gets it, in the right order, without thinking about it.
#pragma once

#include <ostream>

#include <doctest/doctest.h>
