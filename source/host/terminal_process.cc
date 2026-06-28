//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/terminal_process.h"

#include "base/logging.h"

#if defined(Q_OS_WINDOWS)
#include "host/terminal_process_win.h"
#elif defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include "host/terminal_process_unix.h"
#endif

//--------------------------------------------------------------------------------------------------
TerminalProcess::TerminalProcess(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
// static
TerminalProcess* TerminalProcess::create(QObject* parent)
{
#if defined(Q_OS_WINDOWS)
    if (!TerminalProcessWin::isSupported())
    {
        LOG(ERROR) << "Terminal sessions require Windows 10 1809 or newer";
        return nullptr;
    }
    return new TerminalProcessWin(parent);
#elif defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    return new TerminalProcessUnix(parent);
#else
    NOTIMPLEMENTED();
    return nullptr;
#endif
}
