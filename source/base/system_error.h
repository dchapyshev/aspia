//
// SmartCafe Project
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

#ifndef BASE_SYSTEM_ERROR_H
#define BASE_SYSTEM_ERROR_H

#include <QString>

namespace base {

class SystemError
{
public:
#if defined(Q_OS_WINDOWS)
    using Code = unsigned long;
#elif defined(Q_OS_UNIX)
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
    QString toString();

    static QString toString(Code code);

private:
    Code code_;
};

} // namespace base

#endif // BASE_SYSTEM_ERROR_H
