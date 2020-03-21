//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/user_util.h"

#include <cwctype>

namespace common {

namespace {

bool isValidUserNameChar(char16_t username_char)
{
    if (std::iswalnum(username_char))
        return true;

    if (username_char == '.' ||
        username_char == '_' ||
        username_char == '-')
    {
        return true;
    }

    return false;
}

} // namespace

// static
bool UserUtil::isValidUserName(std::u16string_view username)
{
    size_t length = username.length();

    if (!length || length > kMaxUserNameLength)
        return false;

    for (size_t i = 0; i < length; ++i)
    {
        if (!isValidUserNameChar(username[i]))
            return false;
    }

    return true;
}

// static
bool UserUtil::isValidPassword(std::u16string_view password)
{
    size_t length = password.length();

    if (length < kMinPasswordLength || length > kMaxPasswordLength)
        return false;

    return true;
}

// static
bool UserUtil::isSafePassword(std::u16string_view password)
{
    size_t length = password.length();

    if (length < kSafePasswordLength)
        return false;

    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;

    for (size_t i = 0; i < length; ++i)
    {
        char16_t character = password[i];

        if (std::iswupper(character))
            has_upper = true;

        if (std::iswlower(character))
            has_lower = true;

        if (std::iswdigit(character))
            has_digit = true;
    }

    return has_upper && has_lower && has_digit;
}

} // namespace common
