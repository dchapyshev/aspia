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

#include "host/desktop_manager.h"

#include <QCoreApplication>
#include <QDir>

#include "base/application.h"
#include "base/logging.h"
#include "base/ipc/ipc_server.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_impersonator.h"
#include "base/win/session_status.h"
#include <UserEnv.h>
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include <signal.h>
#include <spawn.h>
#endif // defined(Q_OS_LINUX)

namespace host {

namespace {

constexpr std::chrono::milliseconds kRestartTimeout { 3000 };
constexpr std::chrono::milliseconds kAttachTimeout { 15000 };

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
// Creates a copy of the current process token for the given |session_id| so it can be used to
// launch a process in that session.
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
DesktopManager::DesktopManager(QObject* parent)
    : QObject(parent),
      restart_timer_(new QTimer(this)),
      attach_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";
    instance_ = this;

    connect(base::Application::instance(), &base::Application::sig_sessionEvent,
            this, &DesktopManager::onUserSessionEvent);

    restart_timer_->setInterval(kRestartTimeout);
    restart_timer_->setSingleShot(true);

    attach_timer_->setInterval(kAttachTimeout);
    attach_timer_->setSingleShot(true);

    connect(restart_timer_, &QTimer::timeout, this, &DesktopManager::onRestartTimeout);
    connect(attach_timer_, &QTimer::timeout, this, &DesktopManager::onAttachTimeout);
}

//--------------------------------------------------------------------------------------------------
DesktopManager::~DesktopManager()
{
    LOG(INFO) << "Dtor";
    instance_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
DesktopManager* DesktopManager::instance()
{
    return instance_;
}

//--------------------------------------------------------------------------------------------------
// static
QString DesktopManager::filePath()
{
    QString file_path = QCoreApplication::applicationDirPath();
    file_path.append(QLatin1Char('/'));
    file_path.append(kDesktopAgentFile);
    return QDir::toNativeSeparators(file_path);
}

//--------------------------------------------------------------------------------------------------
base::SessionId DesktopManager::sessionId() const
{
    return session_id_;
}

//--------------------------------------------------------------------------------------------------
const QString& DesktopManager::ipcChannelName() const
{
    return ipc_channel_name_;
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::start()
{
    if (process_)
    {
        LOG(ERROR) << "Desktop session process is already started";
        return;
    }

    attach(FROM_HERE, base::activeConsoleSessionId());
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onClientStarted()
{
    ++client_count_;
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onClientFinished()
{
    CHECK(client_count_ > 0);
    --client_count_;

    if (client_count_)
        return;

    LOG(INFO) << "Last desktop client disconnected";

    base::SessionId session_id = base::activeConsoleSessionId();
    if (session_id != session_id_)
    {
        dettach(FROM_HERE);
        attach(FROM_HERE, session_id);
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onUserSessionEvent(quint32 event_type, quint32 session_id)
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "Session event:" << base::sessionStatusToString(event_type)
              << "session id:" << session_id;

    switch (event_type)
    {
        case WTS_CONSOLE_CONNECT:
        {
            if (!is_console_)
            {
                LOG(INFO) << "Ignore event for non-console session";
                return;
            }

            attach(FROM_HERE, session_id);
        }
        break;

        case WTS_CONSOLE_DISCONNECT:
        {
            if (!is_console_)
            {
                LOG(INFO) << "Ignore event for non-console session";
                return;
            }

            dettach(FROM_HERE);
        }
        break;

        case WTS_REMOTE_DISCONNECT:
        {
            if (is_console_)
            {
                LOG(INFO) << "Ignore event for console session";
                return;
            }

            dettach(FROM_HERE);
            attach(FROM_HERE, base::activeConsoleSessionId());
        }
        break;

        default:
            // Ignore other events.
            break;
    }
#else
    NOTIMPLEMENTED();
#endif
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onProcessStateChanged(ProcessState state)
{
    LOG(INFO) << "Process state changed:" << state << "(" << session_id_ << ")";

    switch (state)
    {
        case ProcessState::STARTING:
        {
            // Nothing
        }
        break;

        case ProcessState::STARTED:
        {
            attach_timer_->stop();
            emit sig_attached(ipc_channel_name_);
        }
        break;

        case ProcessState::STOPPED:
        {
            // We don't handle process termination in any way. Instead, we wait for session events
            // in onUserSessionEvent slot to restart.
        }
        break;

        case ProcessState::ERROR_OCURRED:
        {
            // An error occurred while starting the process. Detach the session and start the timer
            // to try to launch it again.
            dettach(FROM_HERE);
            restart_timer_->start();
        }
        break;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onRestartTimeout()
{
    base::SessionId session_id = session_id_;
    dettach(FROM_HERE);
    attach(FROM_HERE, session_id);
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onAttachTimeout()
{
    LOG(INFO) << "Attach timeout. Restarting after" << kRestartTimeout.count() << "ms";
    restart_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::attach(const base::Location& location, base::SessionId session_id)
{
    if (process_)
    {
        LOG(INFO) << "Session already attached (session_id" << session_id_ << "from" << location << ")";
        return;
    }

    LOG(INFO) << "Attach to session" << session_id << "from" << location;

    session_id_ = session_id;
    is_console_ = session_id == base::activeConsoleSessionId();
    ipc_channel_name_ = base::IpcServer::createUniqueId();

    attach_timer_->start();
    startProcess(session_id, ipc_channel_name_);
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::dettach(const base::Location& location)
{
    if (!process_)
    {
        LOG(INFO) << "Session already dettached (session_id" << session_id_ << "from" << location << ")";
        return;
    }

    LOG(INFO) << "Dettach from session" << session_id_ << "from" << location;

    session_id_ = base::kInvalidSessionId;
    ipc_channel_name_.clear();
    attach_timer_->stop();

    stopProcess();
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::startProcess(base::SessionId session_id, const QString& ipc_channel_name)
{
    if (process_state_ != ProcessState::STOPPED)
    {
        LOG(ERROR) << "Desktop process already started";
        return;
    }

    onProcessStateChanged(ProcessState::STARTING);

    if (session_id == base::kInvalidSessionId)
    {
        LOG(ERROR) << "An attempt was detected to start a process in a INVALID session (session_id:"
                   << session_id << "channel_name:" << ipc_channel_name << ")";
        onProcessStateChanged(ProcessState::ERROR_OCURRED);
        return;
    }

#if defined(Q_OS_WINDOWS)
    if (session_id == base::kServiceSessionId)
    {
        LOG(ERROR) << "An attempt was detected to start a process in a SERVICES session ("
                   << "session_id:" << session_id << "ipc_channel_name:" << ipc_channel_name << ")";
        onProcessStateChanged(ProcessState::ERROR_OCURRED);
        return;
    }

    base::ScopedHandle session_token;
    if (!createSessionToken(session_id, &session_token))
    {
        LOG(ERROR) << "createSessionToken failed (session_id:" << session_id << "ipc_channel_name:"
                   << ipc_channel_name << ")";
        onProcessStateChanged(ProcessState::ERROR_OCURRED);
        return;
    }

    QString command_line = filePath() + " --channel_id " + ipc_channel_name;
    base::ScopedHandle process_handle;
    base::ScopedHandle thread_handle;

    if (!startProcessWithToken(session_token, command_line, &process_handle, &thread_handle))
    {
        LOG(ERROR) << "startProcessWithToken failed (session_id:" << session_id << "channel_name:"
                   << ipc_channel_name << ")";
        onProcessStateChanged(ProcessState::ERROR_OCURRED);
        return;
    }

    process_ = std::move(process_handle);
    thread_ = std::move(thread_handle);

    process_notifier_ = new QWinEventNotifier(process_, this);
    connect(process_notifier_, &QWinEventNotifier::activated, [this](HANDLE /* handle */)
    {
        process_notifier_->setEnabled(false);
        onProcessStateChanged(ProcessState::STOPPED);
    });

    onProcessStateChanged(ProcessState::STARTED);
    process_notifier_->setEnabled(true);
#elif defined(Q_OS_LINUX)
    // TODO: Notifications about the completion of the process are not implemented for Linux.

    std::error_code ignored_error;
    std::filesystem::directory_iterator it("/usr/share/xsessions/", ignored_error);
    if (it == std::filesystem::end(it))
    {
        LOG(ERROR) << "No X11 sessions";
        onProcessStateChanged(ProcessState::ERROR_OCURRED);
        return;
    }

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("who", "r"), pclose);
    if (!pipe)
    {
        LOG(ERROR) << "Unable to open pipe";
        onProcessStateChanged(ProcessState::ERROR_OCURRED);
        return;
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
                .arg(user_name, filePath(), channel_name).toLocal8Bit();

        LOG(INFO) << "Start desktop session agent:" << command_line;

        char sh_name[] = "sh";
        char sh_arguments[] = "-c";
        char* argv[] = { sh_name, sh_arguments, command_line.data(), nullptr };

        pid_t pid;
        if (posix_spawn(&pid, "/bin/sh", nullptr, nullptr, argv, environ) != 0)
        {
            PLOG(ERROR) << "Unable to start process";
            onProcessStateChanged(ProcessState::ERROR_OCURRED);
            return;
        }

        pid_ = pid;
        onProcessStateChanged(ProcessState::STARTED);
        return;
    }

    LOG(ERROR) << "Connected X sessions not found";

    QByteArray command_line =
        QString("sudo DISPLAY=':0' -u root %1 --channel_id=%2 &")
            .arg(filePath(), channel_name).toLocal8Bit();

    LOG(INFO) << "Start desktop session agent:" << command_line;

    char sh_name[] = "sh";
    char sh_arguments[] = "-c";
    char* argv[] = { sh_name, sh_arguments, command_line.data(), nullptr };

    pid_t pid;
    if (posix_spawn(&pid, "/bin/sh", nullptr, nullptr, argv, environ) != 0)
    {
        PLOG(ERROR) << "Unable to start process";
        onProcessStateChanged(ProcessState::ERROR_OCURRED);
        return;
    }

    pid_ = pid;
    onProcessStateChanged(ProcessState::STARTED);
#else
    NOTIMPLEMENTED();
    onProcessStateChanged(ProcessState::ERROR_OCURRED);
#endif
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::stopProcess()
{
#if defined(Q_OS_WINDOWS)
    if (process_.isValid())
    {
        if (!TerminateProcess(process_, 0))
        {
            PLOG(ERROR) << "TerminateProcess failed";
        }
    }

    if (process_notifier_)
    {
        process_notifier_->setEnabled(false);
        process_notifier_->deleteLater();
        process_notifier_ = nullptr;
    }

    onProcessStateChanged(ProcessState::STOPPED);
#elif defined(Q_OS_LINUX)
    if (::kill(pid_, SIGKILL) != 0)
    {
        PLOG(ERROR) << "kill failed";
    }
#else
    NOTIMPLEMENTED();
#endif
}

//--------------------------------------------------------------------------------------------------
// static
thread_local DesktopManager* DesktopManager::instance_ = nullptr;

} // namespace host
