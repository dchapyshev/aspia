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

#ifndef BASE_SCOPED_CLEAR_LAST_ERROR_H
#define BASE_SCOPED_CLEAR_LAST_ERROR_H

#include <QtGlobal>

namespace base {

// ScopedClearLastError stores and resets the value of thread local error codes
// (errno, GetLastError()), and restores them in the destructor. This is useful
// to avoid side effects on these values in instrumentation functions that
// interact with the OS.

// Common implementation of ScopedClearLastError for all platforms. Use
// ScopedClearLastError instead.
class ScopedClearLastErrorBase
{
public:
    ScopedClearLastErrorBase();
    ~ScopedClearLastErrorBase();

private:
    const int last_errno_;
    Q_DISABLE_COPY(ScopedClearLastErrorBase)
};

#if defined(Q_OS_WINDOWS)

// Windows specific implementation of ScopedClearLastError.
class ScopedClearLastError final : public ScopedClearLastErrorBase
{
public:
    ScopedClearLastError();
    ~ScopedClearLastError();

private:
    const unsigned long last_error_;
    Q_DISABLE_COPY(ScopedClearLastError)
};

#elif defined(Q_OS_UNIX)
using ScopedClearLastError = ScopedClearLastErrorBase;
#else
#error Platform support not implemented
#endif

} // namespace base

#endif // BASE_SCOPED_CLEAR_LAST_ERROR_H
