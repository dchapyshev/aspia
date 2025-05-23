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

#ifndef BASE_WIN_SAFE_MODE_UTIL_H
#define BASE_WIN_SAFE_MODE_UTIL_H

#include "base/macros_magic.h"

#include <string>

namespace base::win {

class SafeModeUtil
{
public:
    static bool setSafeMode(bool enable);
    static bool setSafeModeService(std::u16string_view service_name, bool enable);

private:
    DISALLOW_COPY_AND_ASSIGN(SafeModeUtil);
};

} // namespace base::win

#endif // BASE_WIN_SAFE_MODE_UTIL_H
