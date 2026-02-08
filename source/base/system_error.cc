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

#include "base/system_error.h"

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#elif defined(Q_OS_UNIX)
#include <cstring>
#include <cerrno>
#endif

namespace base {

//--------------------------------------------------------------------------------------------------
SystemError::SystemError(Code code)
    : code_(code)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
// static
SystemError SystemError::last()
{
#if defined(Q_OS_WINDOWS)
    return SystemError(GetLastError());
#elif defined(Q_OS_UNIX)
    return SystemError(errno);
#else
#error Platform support not implemented
#endif
}

//--------------------------------------------------------------------------------------------------
SystemError::Code SystemError::code() const
{
    return code_;
}

//--------------------------------------------------------------------------------------------------
QString SystemError::toString()
{
    return toString(code_);
}

//--------------------------------------------------------------------------------------------------
// static
QString SystemError::toString(Code code)
{
#if defined(Q_OS_WINDOWS)
    constexpr int kErrorMessageBufferSize = 256;
    wchar_t msgbuf[kErrorMessageBufferSize];

    DWORD len = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                               code, 0, msgbuf, static_cast<DWORD>(std::size(msgbuf)), nullptr);
    if (len)
        return QString::fromWCharArray(msgbuf, len).trimmed() + ' ' + QString::number(code, 16);

    return QString("Error (0x%1) while retrieving error. (0x%2)").arg(GetLastError()).arg(code);
#elif defined(Q_OS_UNIX)
    return std::strerror(code);
#else
#error Platform support not implemented
#endif
}

} // namespace base
