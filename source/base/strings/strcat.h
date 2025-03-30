//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef BASE_STRINGS_STRCAT_H
#define BASE_STRINGS_STRCAT_H

#include <initializer_list>
#include <string>

namespace base {

// strCat ----------------------------------------------------------------------
//
// strCat is a function to perform concatenation on a sequence of strings.
// It is preferrable to a sequence of "a + b + c" because it is both faster and generates less
// code.
//
//   std::string result = base::strCat({"foo ", result, "\nfoo ", bar});
//
// MORE INFO
//
// strCat can see all arguments at once, so it can allocate one return buffer
// of exactly the right size and copy once, as opposed to a sequence of
// operator+ which generates a series of temporary strings, copying as it goes.
// And by using std::string_view arguments, strCat can avoid creating temporary
// string objects for char* constants.

std::string strCat(std::initializer_list<std::string_view> pieces);
std::u16string strCat(std::initializer_list<std::u16string_view> pieces);

// strAppend -------------------------------------------------------------------
//
// Appends a sequence of strings to a destination. Prefer:
//   strAppend(&foo, ...);
// over:
//   foo += strCat(...);
// because it avoids a temporary string allocation and copy.

void strAppend(std::string* dest, std::initializer_list<std::string_view> pieces);
void strAppend(std::u16string* dest, std::initializer_list<std::u16string_view> pieces);

} // namespace base

#endif // BASE_STRINGS_STRCAT_H
