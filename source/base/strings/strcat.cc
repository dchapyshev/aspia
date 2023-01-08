//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/strings/strcat.h"

#include <algorithm>

namespace base {

namespace {

// Reserves an additional amount of capacity in the given string, growing by at least 2x if
// necessary. Used by strAppendT().
//
// The "at least 2x" growing rule duplicates the exponential growth of std::string. The problem is
// that most implementations of reserve() will grow exactly to the requested amount instead of
// exponentially growing like would happen when appending normally. If we didn't do this, an append
// after the call to strAppend() would definitely cause a reallocation, and loops with strAppend()
// calls would have O(n^2) complexity to execute. Instead, we want strAppend() to have the same
// semantics as std::string::append().
template <typename String>
void reserveAdditionalIfNeeded(String* str, typename String::size_type additional)
{
    const size_t required = str->size() + additional;

    // Check whether we need to reserve additional capacity at all.
    if (required <= str->capacity())
        return;

    str->reserve(std::max(required, str->capacity() * 2));
}

template <typename DestString, typename InputString>
void strAppendT(DestString* dest, std::initializer_list<InputString> pieces)
{
    size_t additional_size = 0;

    for (const auto& cur : pieces)
        additional_size += cur.size();

    reserveAdditionalIfNeeded(dest, additional_size);

    for (const auto& cur : pieces)
        dest->append(cur.data(), cur.size());
}

}  // namespace

std::string strCat(std::initializer_list<std::string_view> pieces)
{
    std::string result;
    strAppendT(&result, pieces);
    return result;
}

std::u16string strCat(std::initializer_list<std::u16string_view> pieces)
{
    std::u16string result;
    strAppendT(&result, pieces);
    return result;
}

void strAppend(std::string* dest, std::initializer_list<std::string_view> pieces)
{
    strAppendT(dest, pieces);
}

void strAppend(std::u16string* dest, std::initializer_list<std::u16string_view> pieces)
{
    strAppendT(dest, pieces);
}

} // namespace base
