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

#ifndef HOST_DESKTOP_SESSION_PROCESS_H
#define HOST_DESKTOP_SESSION_PROCESS_H

#include "build/build_config.h"
#include "base/macros_magic.h"
#include "base/session_id.h"

#if defined(OS_WIN)
#include "base/win/scoped_object.h"
#endif // defined(OS_WIN)

#include <filesystem>
#include <memory>

#include <QString>

namespace ipc {
class Channel;
} // namespace ipc

namespace host {

class DesktopSessionProcess
{
public:
    ~DesktopSessionProcess();

    static std::unique_ptr<DesktopSessionProcess> create(
        base::SessionId session_id, const QString& channel_id);
    static QString filePath();

    void kill();

private:
#if defined(OS_WIN)
    DesktopSessionProcess(base::win::ScopedHandle&& process, base::win::ScopedHandle&& thread);
#elif defined(OS_LINUX)
    explicit DesktopSessionProcess(pid_t pid);
#else
    DesktopSessionProcess();
#endif

#if defined(OS_WIN)
    base::win::ScopedHandle process_;
    base::win::ScopedHandle thread_;
#endif // defined(OS_WIN)

#if defined(OS_LINUX)
    const pid_t pid_;
#endif // defined(OS_LINUX)

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionProcess);
};

} // namespace host

#endif // HOST_DESKTOP_SESSION_PROCESS_H
