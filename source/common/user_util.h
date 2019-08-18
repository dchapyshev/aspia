//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON__USER_UTIL_H
#define COMMON__USER_UTIL_H

#include "base/macros_magic.h"

#include <string>

namespace common {

class UserUtil
{
public:
    static const size_t kMaxUserNameLength = 64;
    static const size_t kMinPasswordLength = 1;
    static const size_t kMaxPasswordLength = 64;
    static const size_t kSafePasswordLength = 8;

    static bool isValidUserName(std::u16string_view username);
    static bool isValidPassword(std::u16string_view password);
    static bool isSafePassword(std::u16string_view password);

private:
    DISALLOW_COPY_AND_ASSIGN(UserUtil);
};

} // namespace common

#endif // COMMON__USER_UTIL_H
