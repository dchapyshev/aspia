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

#include <QString>

#include <memory>

#include "base/session_id.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
#endif // defined(Q_OS_WINDOWS)

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
#if defined(Q_OS_WINDOWS)
    DesktopSessionProcess(base::ScopedHandle&& process, base::ScopedHandle&& thread);
#elif defined(Q_OS_LINUX)
    explicit DesktopSessionProcess(pid_t pid);
#else
    DesktopSessionProcess();
#endif

#if defined(Q_OS_WINDOWS)
    base::ScopedHandle process_;
    base::ScopedHandle thread_;
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
    const pid_t pid_;
#endif // defined(Q_OS_LINUX)

    Q_DISABLE_COPY(DesktopSessionProcess)
};

} // namespace host

#endif // HOST_DESKTOP_SESSION_PROCESS_H
