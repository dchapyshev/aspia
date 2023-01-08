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

#ifndef BASE_SCOPED_CLEAR_LAST_ERROR_H
#define BASE_SCOPED_CLEAR_LAST_ERROR_H

#include "base/macros_magic.h"
#include "build/build_config.h"

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
    DISALLOW_COPY_AND_ASSIGN(ScopedClearLastErrorBase);
};

#if defined(OS_WIN)

// Windows specific implementation of ScopedClearLastError.
class ScopedClearLastError : public ScopedClearLastErrorBase
{
public:
    ScopedClearLastError();
    ~ScopedClearLastError();

private:
    const unsigned long last_error_;
    DISALLOW_COPY_AND_ASSIGN(ScopedClearLastError);
};

#elif defined(OS_POSIX)
using ScopedClearLastError = ScopedClearLastErrorBase;
#else
#error Platform support not implemented
#endif // defined(OS_WIN)

} // namespace base

#endif // BASE_SCOPED_CLEAR_LAST_ERROR_H
