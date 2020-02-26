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

#ifndef BASE__SYSTEM_ERROR_H
#define BASE__SYSTEM_ERROR_H

#include "build/build_config.h"

#include <string>

namespace base {

class SystemError
{
public:
#if defined(OS_WIN)
    using Code = unsigned long;
#elif defined(OS_POSIX)
    using Code = int;
#else
#error Platform support not implemented
#endif

    explicit SystemError(Code code);
    ~SystemError() = default;

    SystemError(const SystemError& other) = default;
    SystemError& operator=(const SystemError& other) = default;

    // Alias for GetLastError() on Windows and errno on POSIX. Avoids having to pull in Windows.h
    // just for GetLastError() and DWORD.
    static SystemError last();

    // Returns an error code.
    Code code() const;

    // Returns a string description of the error in UTF-8 encoding.
    std::string toString();

    static std::string toString(Code code);

private:
    const Code code_;
};

} // namespace base

#endif // BASE__SYSTEM_ERROR_H
