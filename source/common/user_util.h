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

#ifndef ASPIA_COMMON__USER_UTIL_H_
#define ASPIA_COMMON__USER_UTIL_H_

#include <QString>

#include "base/macros_magic.h"

namespace aspia {

class UserUtil
{
public:
    static const int kMaxUserNameLength = 64;
    static const int kMinPasswordLength = 1;
    static const int kMaxPasswordLength = 64;
    static const int kSafePasswordLength = 8;

    static bool isValidUserName(const QString& username);
    static bool isValidPassword(const QString& password);
    static bool isSafePassword(const QString& password);

private:
    DISALLOW_COPY_AND_ASSIGN(UserUtil);
};

} // namespace aspia

#endif // ASPIA_COMMON__USER_UTIL_H_
