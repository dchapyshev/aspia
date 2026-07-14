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

#include "base/process_util.h"

#include "base/logging.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"

#include <qt_windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include <QFile>

#include <limits.h>
#endif // defined(Q_OS_LINUX)

#if defined(Q_OS_MACOS)
#include <libproc.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#endif // defined(Q_OS_MACOS)

#if defined(Q_OS_UNIX)
#include <unistd.h>
#endif // defined(Q_OS_UNIX)

//--------------------------------------------------------------------------------------------------
// static
quint32 ProcessUtil::currentProcessId()
{
#if defined(Q_OS_WINDOWS)
    return static_cast<quint32>(GetCurrentProcessId());
#elif defined(Q_OS_UNIX)
    return static_cast<quint32>(getpid());
#else
    return 0;
#endif
}

//--------------------------------------------------------------------------------------------------
// static
quint32 ProcessUtil::parentProcessId(quint32 pid)
{
#if defined(Q_OS_WINDOWS)
    ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snapshot.isValid())
    {
        PLOG(ERROR) << "CreateToolhelp32Snapshot failed";
        return 0;
    }

    PROCESSENTRY32W entry;
    memset(&entry, 0, sizeof(entry));
    entry.dwSize = sizeof(entry);

    if (!Process32FirstW(snapshot, &entry))
    {
        PLOG(ERROR) << "Process32FirstW failed";
        return 0;
    }

    do
    {
        if (entry.th32ProcessID == pid)
            return static_cast<quint32>(entry.th32ParentProcessID);
    }
    while (Process32NextW(snapshot, &entry));

    LOG(ERROR) << "Process not found (pid:" << pid << ")";
    return 0;
#elif defined(Q_OS_LINUX)
    // /proc/<pid>/status has line-based "Key: value" format. Find "PPid:" and parse the integer.
    QFile file(QString("/proc/%1/status").arg(pid));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        PLOG(ERROR) << "Unable to open /proc/" << pid << "/status";
        return 0;
    }

    QByteArray line;
    while (!(line = file.readLine()).isEmpty())
    {
        if (!line.startsWith("PPid:"))
            continue;

        return line.mid(5).trimmed().toUInt();
    }

    LOG(ERROR) << "PPid not found in /proc/" << pid << "/status";
    return 0;
#elif defined(Q_OS_MACOS)
    struct kinfo_proc info;
    size_t len = sizeof(info);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(pid) };

    if (sysctl(mib, 4, &info, &len, nullptr, 0) != 0 || len == 0)
    {
        PLOG(ERROR) << "sysctl(KERN_PROC_PID) failed (pid:" << pid << ")";
        return 0;
    }

    return static_cast<quint32>(info.kp_eproc.e_ppid);
#else
    Q_UNUSED(pid)
    return 0;
#endif
}

//--------------------------------------------------------------------------------------------------
// static
QString ProcessUtil::filePath(quint32 pid)
{
#if defined(Q_OS_WINDOWS)
    ScopedHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!process.isValid())
    {
        PLOG(ERROR) << "OpenProcess failed (pid:" << pid << ")";
        return QString();
    }

    wchar_t buffer[MAX_PATH];
    DWORD size = static_cast<DWORD>(std::size(buffer));
    if (!QueryFullProcessImageNameW(process, 0, buffer, &size))
    {
        PLOG(ERROR) << "QueryFullProcessImageNameW failed (pid:" << pid << ")";
        return QString();
    }

    return QString::fromWCharArray(buffer, static_cast<int>(size));
#elif defined(Q_OS_LINUX)
    char buffer[PATH_MAX];
    ssize_t size = readlink(QString("/proc/%1/exe").arg(pid).toLocal8Bit().constData(),
                            buffer, sizeof(buffer));
    if (size <= 0)
    {
        PLOG(ERROR) << "readlink(/proc/" << pid << "/exe) failed";
        return QString();
    }

    return QString::fromLocal8Bit(buffer, static_cast<int>(size));
#elif defined(Q_OS_MACOS)
    char buffer[PROC_PIDPATHINFO_MAXSIZE];
    int size = proc_pidpath(static_cast<int>(pid), buffer, sizeof(buffer));
    if (size <= 0)
    {
        PLOG(ERROR) << "proc_pidpath failed (pid:" << pid << ")";
        return QString();
    }

    return QString::fromLocal8Bit(buffer, size);
#else
    Q_UNUSED(pid)
    return QString();
#endif
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
// static
bool ProcessUtil::isProcessElevated()
{
    ScopedHandle token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.recieve()))
    {
        PLOG(ERROR) << "OpenProcessToken failed";
        return false;
    }

    TOKEN_ELEVATION elevation;
    DWORD size;

    if (!GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size))
    {
        PLOG(ERROR) << "GetTokenInformation failed";
        return false;
    }

    return elevation.TokenIsElevated != 0;
}

//--------------------------------------------------------------------------------------------------
// static
bool ProcessUtil::createProcess(const QString& program, const QString& arguments, ExecuteMode mode)
{
    SHELLEXECUTEINFOW sei;
    memset(&sei, 0, sizeof(sei));

    sei.cbSize = sizeof(sei);
    sei.lpVerb = ((mode == ExecuteMode::ELEVATE) ? L"runas" : L"open");
    sei.lpFile = qUtf16Printable(program);
    sei.hwnd = nullptr;
    sei.nShow = SW_SHOW;
    sei.lpParameters = qUtf16Printable(arguments);

    if (!ShellExecuteExW(&sei))
    {
        PLOG(ERROR) << "ShellExecuteExW failed";
        return false;
    }

    return true;
}
#endif // defined(Q_OS_WINDOWS)
