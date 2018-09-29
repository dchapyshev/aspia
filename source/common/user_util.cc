//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

namespace aspia {

namespace {

bool isValidUserNameChar(const QChar& username_char)
{
    if (username_char.isLetter())
        return true;

    if (username_char.isDigit())
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
bool UserUtil::isValidUserName(const QString& username)
{
    int length = username.length();

    if (length <= 0 || length > kMaxUserNameLength)
        return false;

    for (int i = 0; i < length; ++i)
    {
        if (!isValidUserNameChar(username[i]))
            return false;
    }

    return true;
}

// static
bool UserUtil::isValidPassword(const QString& password)
{
    int length = password.length();

    if (length < kMinPasswordLength || length > kMaxPasswordLength)
        return false;

    return true;
}

// static
bool UserUtil::isSafePassword(const QString& password)
{
    int length = password.length();

    if (length < kSafePasswordLength)
        return false;

    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;

    for (int i = 0; i < length; ++i)
    {
        const QChar& character = password[i];

        has_upper |= character.isUpper();
        has_lower |= character.isLower();
        has_digit |= character.isDigit();
    }

    return has_upper && has_lower && has_digit;
}

} // namespace aspia
