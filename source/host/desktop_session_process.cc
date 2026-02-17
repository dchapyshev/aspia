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

#include "host/desktop_session_process.h"

#include <QCoreApplication>
#include <QDir>

#if defined(Q_OS_LINUX)
#include <signal.h>
#include <spawn.h>
#endif // defined(Q_OS_LINUX)

#include "base/logging.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_impersonator.h"
#include <UserEnv.h>
#endif // defined(Q_OS_WINDOWS)

namespace host {

namespace {

#if defined(Q_OS_LINUX)
const char kDesktopAgentFile[] = "aspia_desktop_agent";
#endif

#if defined(Q_OS_WINDOWS)
// Name of the default session desktop.
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";
const char kDesktopAgentFile[] = "aspia_desktop_agent.exe";

//--------------------------------------------------------------------------------------------------
bool copyProcessToken(DWORD desired_access, base::ScopedHandle* token_out)
{
    base::ScopedHandle process_token;

    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_DUPLICATE | desired_access,
                          process_token.recieve()))
    {
        PLOG(ERROR) << "OpenProcessToken failed";
        return false;
    }

    if (!DuplicateTokenEx(process_token,
                          desired_access,
                          nullptr,
                          SecurityImpersonation,
                          TokenPrimary,
                          token_out->recieve()))
    {
        PLOG(ERROR) << "DuplicateTokenEx failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool createPrivilegedToken(base::ScopedHandle* token_out)
{
    base::ScopedHandle privileged_token;
    const DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
        TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!copyProcessToken(desired_access, &privileged_token))
    {
        LOG(ERROR) << "copyProcessToken failed";
        return false;
    }

    // Get the LUID for the SE_TCB_NAME privilege.
    TOKEN_PRIVILEGES state;
    state.PrivilegeCount = 1;
    state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_TCB_NAME, &state.Privileges[0].Luid))
    {
        PLOG(ERROR) << "LookupPrivilegeValueW failed";
        return false;
    }

    // Enable the SE_TCB_NAME privilege.
    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0, nullptr, nullptr))
    {
        PLOG(ERROR) << "AdjustTokenPrivileges failed";
        return false;
    }

    token_out->reset(privileged_token.release());
    return true;
}

//--------------------------------------------------------------------------------------------------
// Creates a copy of the current process token for the given |session_id| so
// it can be used to launch a process in that session.
bool createSessionToken(DWORD session_id, base::ScopedHandle* token_out)
{
    base::ScopedHandle session_token;
    const DWORD desired_access = TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID |
        TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!copyProcessToken(desired_access, &session_token))
    {
        LOG(ERROR) << "copyProcessToken failed";
        return false;
    }

    base::ScopedHandle privileged_token;

    if (!createPrivilegedToken(&privileged_token))
    {
        LOG(ERROR) << "createPrivilegedToken failed";
        return false;
    }

    base::ScopedImpersonator impersonator;

    if (!impersonator.loggedOnUser(privileged_token))
    {
        LOG(ERROR) << "Failed to impersonate thread";
        return false;
    }

    // Change the session ID of the token.
    if (!SetTokenInformation(session_token, TokenSessionId, &session_id, sizeof(session_id)))
    {
        PLOG(ERROR) << "SetTokenInformation failed";
        return false;
    }

    DWORD ui_access = 1;
    if (!SetTokenInformation(session_token, TokenUIAccess, &ui_access, sizeof(ui_access)))
    {
        PLOG(ERROR) << "SetTokenInformation failed";
        return false;
    }

    token_out->reset(session_token.release());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool startProcessWithToken(HANDLE token,
                           const QString& command_line,
                           base::ScopedHandle* process,
                           base::ScopedHandle* thread)
{
    STARTUPINFOW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));

    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = const_cast<wchar_t*>(kDefaultDesktopName);

    void* environment = nullptr;

    if (!CreateEnvironmentBlock(&environment, token, FALSE))
    {
        PLOG(ERROR) << "CreateEnvironmentBlock failed";
        return false;
    }

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    if (!CreateProcessAsUserW(token,
                              nullptr,
                              const_cast<wchar_t*>(qUtf16Printable(command_line)),
                              nullptr,
                              nullptr,
                              FALSE,
                              CREATE_UNICODE_ENVIRONMENT | HIGH_PRIORITY_CLASS,
                              environment,
                              nullptr,
                              &startup_info,
                              &process_info))
    {
        PLOG(ERROR) << "CreateProcessAsUserW failed";
        if (!DestroyEnvironmentBlock(environment))
        {
            PLOG(ERROR) << "DestroyEnvironmentBlock failed";
        }
        return false;
    }

    thread->reset(process_info.hThread);
    process->reset(process_info.hProcess);

    if (!DestroyEnvironmentBlock(environment))
    {
        PLOG(ERROR) << "DestroyEnvironmentBlock failed";
    }

    return true;
}
#endif // defined(Q_OS_WINDOWS)

} // namespace

//--------------------------------------------------------------------------------------------------
#if defined(Q_OS_WINDOWS)
DesktopSessionProcess::DesktopSessionProcess(
    base::ScopedHandle&& process, base::ScopedHandle&& thread)
    : process_(std::move(process)),
      thread_(std::move(thread))
{
    LOG(INFO) << "Ctor";
}
#elif defined(Q_OS_LINUX)
DesktopSessionProcess::DesktopSessionProcess(pid_t pid)
    : pid_(pid)
{
    LOG(INFO) << "Ctor";
}
#else
DesktopSessionProcess::DesktopSessionProcess()
{
    LOG(INFO) << "Ctor";
}
#endif

//--------------------------------------------------------------------------------------------------
DesktopSessionProcess::~DesktopSessionProcess()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<DesktopSessionProcess> DesktopSessionProcess::create(
    base::SessionId session_id, const QString& channel_id)
{
    if (session_id == base::kInvalidSessionId)
    {
        LOG(ERROR) << "An attempt was detected to start a process in a INVALID session (session_id="
                   << session_id << "channel_id=" << channel_id << ")";
        return nullptr;
    }

#if defined(Q_OS_WINDOWS)
    if (session_id == base::kServiceSessionId)
    {
        LOG(ERROR) << "An attempt was detected to start a process in a SERVICES session ("
                   << "session_id=" << session_id << "channel_id=" << channel_id << ")";
        return nullptr;
    }

    base::ScopedHandle session_token;
    if (!createSessionToken(session_id, &session_token))
    {
        LOG(ERROR) << "createSessionToken failed (session_id=" << session_id << "channel_id="
                   << channel_id << ")";
        return nullptr;
    }

    QString command_line = filePath() + " --channel_id " + channel_id;
    base::ScopedHandle process_handle;
    base::ScopedHandle thread_handle;

    if (!startProcessWithToken(session_token, command_line, &process_handle, &thread_handle))
    {
        LOG(ERROR) << "startProcessWithToken failed (session_id=" << session_id << "channel_id="
                   << channel_id << ")";
        return nullptr;
    }

    return std::unique_ptr<DesktopSessionProcess>(
        new DesktopSessionProcess(std::move(process_handle), std::move(thread_handle)));
#elif defined(Q_OS_LINUX)
    std::error_code ignored_error;
    std::filesystem::directory_iterator it("/usr/share/xsessions/", ignored_error);
    if (it == std::filesystem::end(it))
    {
        LOG(ERROR) << "No X11 sessions";
        return nullptr;
    }

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("who", "r"), pclose);
    if (!pipe)
    {
        LOG(ERROR) << "Unable to open pipe";
        return nullptr;
    }

    LOG(INFO) << "WHO:";
    std::array<char, 512> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe.get()))
    {
        QString line = QString::fromLocal8Bit(buffer.data()).toLower();
        LOG(INFO) << line;
        if (!line.contains(":0"))
            continue;

        QStringList splitted = line.split(' ', Qt::SkipEmptyParts);
        if (splitted.isEmpty())
            continue;

        QString user_name = splitted.front();
        QByteArray command_line =
            QString("sudo DISPLAY=':0' -u %1 %2 --channel_id=%3 &")
                .arg(user_name, filePath(), channel_id).toLocal8Bit();

        LOG(INFO) << "Start desktop session agent:" << command_line;

        char sh_name[] = "sh";
        char sh_arguments[] = "-c";
        char* argv[] = { sh_name, sh_arguments, command_line.data(), nullptr };

        pid_t pid;
        if (posix_spawn(&pid, "/bin/sh", nullptr, nullptr, argv, environ) != 0)
        {
            PLOG(ERROR) << "Unable to start process";
            return nullptr;
        }

        return std::unique_ptr<DesktopSessionProcess>(new DesktopSessionProcess(pid));
    }

    LOG(ERROR) << "Connected X sessions not found";

    QByteArray command_line =
        QString("sudo DISPLAY=':0' -u root %1 --channel_id=%2 &")
            .arg(filePath(), channel_id).toLocal8Bit();

    LOG(INFO) << "Start desktop session agent:" << command_line;

    char sh_name[] = "sh";
    char sh_arguments[] = "-c";
    char* argv[] = { sh_name, sh_arguments, command_line.data(), nullptr };

    pid_t pid;
    if (posix_spawn(&pid, "/bin/sh", nullptr, nullptr, argv, environ) != 0)
    {
        PLOG(ERROR) << "Unable to start process";
        return nullptr;
    }

    return std::unique_ptr<DesktopSessionProcess>(new DesktopSessionProcess(pid));
#else
    NOTIMPLEMENTED();
    return std::unique_ptr<DesktopSessionProcess>();
#endif
}

//--------------------------------------------------------------------------------------------------
// static
QString DesktopSessionProcess::filePath()
{
    QString file_path = QCoreApplication::applicationDirPath();
    file_path.append(QLatin1Char('/'));
    file_path.append(kDesktopAgentFile);
    return QDir::toNativeSeparators(file_path);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionProcess::kill()
{
#if defined(Q_OS_WINDOWS)
    if (!process_.isValid())
    {
        LOG(ERROR) << "Invalid process handle";
        return;
    }

    if (!TerminateProcess(process_, 0))
    {
        PLOG(ERROR) << "TerminateProcess failed";
    }
#elif defined(Q_OS_LINUX)
    if (::kill(pid_, SIGKILL) != 0)
    {
        PLOG(ERROR) << "kill failed";
    }
#else
    NOTIMPLEMENTED();
#endif
}

} // namespace host
