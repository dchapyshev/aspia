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

#include "base/scoped_clear_last_error.h"

#include <cerrno>

#if defined(OS_WIN)
#include <Windows.h>
#endif // defined(OS_WIN)

namespace base {

//--------------------------------------------------------------------------------------------------
ScopedClearLastErrorBase::ScopedClearLastErrorBase()
    : last_errno_(errno)
{
    errno = 0;
}

//--------------------------------------------------------------------------------------------------
ScopedClearLastErrorBase::~ScopedClearLastErrorBase()
{
    errno = last_errno_;
}

#if defined(OS_WIN)
//--------------------------------------------------------------------------------------------------
ScopedClearLastError::ScopedClearLastError()
    : last_error_(GetLastError())
{
    SetLastError(0);
}

//--------------------------------------------------------------------------------------------------
ScopedClearLastError::~ScopedClearLastError()
{
    SetLastError(last_error_);
}
#endif // defined(OS_WIN)

} // namespace base
