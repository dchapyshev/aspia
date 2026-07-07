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

#include "host/terminal_client.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
#include "base/win/security_helpers.h"

#include <vector>

#include <AclApi.h>
#include <UserEnv.h>
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <cstdlib>
#include <cstring>

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include <security/pam_appl.h>
#endif // defined(Q_OS_LINUX) || defined(Q_OS_MACOS)

#include "base/location.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/serialization.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "proto/peer.h"

namespace {

// Number of failed credential attempts after which the session is closed.
const int kMaxAuthFailures = 5;

#if defined(Q_OS_WINDOWS)
const char kTerminalAgentFile[] = "aspia_terminal_agent.exe";
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";
#else
const char kTerminalAgentFile[] = "aspia_terminal_agent";
#endif

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
// PAM service used to validate the credentials. The matching policy file is shipped as
// /etc/pam.d/aspia-terminal.
const char kPamServiceName[] = "aspia-terminal";

struct PamConversationData
{
    std::string password;
};

//--------------------------------------------------------------------------------------------------
int pamConversation(int num_msg, const struct pam_message** msg, struct pam_response** resp,
                    void* appdata_ptr)
{
    if (num_msg <= 0 || !appdata_ptr)
        return PAM_CONV_ERR;

    auto* data = reinterpret_cast<PamConversationData*>(appdata_ptr);

    auto* responses = reinterpret_cast<struct pam_response*>(
        calloc(static_cast<size_t>(num_msg), sizeof(struct pam_response)));
    if (!responses)
        return PAM_BUF_ERR;

    for (int i = 0; i < num_msg; ++i)
    {
        switch (msg[i]->msg_style)
        {
            case PAM_PROMPT_ECHO_OFF:
            case PAM_PROMPT_ECHO_ON:
                responses[i].resp = strdup(data->password.c_str());
                break;

            default:
                responses[i].resp = nullptr;
                break;
        }
    }

    *resp = responses;
    return PAM_SUCCESS;
}

//--------------------------------------------------------------------------------------------------
bool pamAuthenticate(const QString& user_name, const QString& password)
{
    PamConversationData data;
    data.password = password.toStdString();

    struct pam_conv conv;
    conv.conv = pamConversation;
    conv.appdata_ptr = &data;

    pam_handle_t* pamh = nullptr;
    int rc = pam_start(kPamServiceName, user_name.toUtf8().constData(), &conv, &pamh);
    if (rc != PAM_SUCCESS)
    {
        LOG(ERROR) << "pam_start failed:" << pam_strerror(pamh, rc);
        return false;
    }

    rc = pam_authenticate(pamh, 0);
    if (rc == PAM_SUCCESS)
        rc = pam_acct_mgmt(pamh, 0);

    if (rc != PAM_SUCCESS)
        LOG(WARNING) << "PAM authentication failed:" << pam_strerror(pamh, rc);

    pam_end(pamh, rc);
    return rc == PAM_SUCCESS;
}

//--------------------------------------------------------------------------------------------------
bool launchAgentAsUser(const QString& user_name, const QString& agent_path, const QString& ipc_channel_id)
{
    const std::string user_name_utf8 = user_name.toStdString();

    struct passwd* pw = getpwnam(user_name_utf8.c_str());
    if (!pw)
    {
        LOG(ERROR) << "Unknown user:" << user_name;
        return false;
    }

    const uid_t uid = pw->pw_uid;
    const gid_t gid = pw->pw_gid;
    const std::string home_dir = pw->pw_dir ? pw->pw_dir : "/";
    const std::string agent_path_utf8 = agent_path.toStdString();
    const std::string channel_id_utf8 = ipc_channel_id.toStdString();

    pid_t pid = fork();
    if (pid < 0)
    {
        PLOG(ERROR) << "fork failed";
        return false;
    }

    if (pid == 0)
    {
        // Child process: drop privileges to the target user and exec the agent.
        if (setsid() == -1)
            _exit(127);

        if (initgroups(user_name_utf8.c_str(), gid) != 0)
            _exit(127);
        if (setgid(gid) != 0)
            _exit(127);
        if (setuid(uid) != 0)
            _exit(127);

        setenv("HOME", home_dir.c_str(), 1);
        setenv("USER", user_name_utf8.c_str(), 1);
        setenv("LOGNAME", user_name_utf8.c_str(), 1);
        setenv(IpcServer::kChannelIdEnvVar, channel_id_utf8.c_str(), 1);

        if (chdir(home_dir.c_str()) != 0)
        {
            // Non-fatal: fall back to the root directory.
            if (chdir("/") != 0)
                _exit(127);
        }

        char* argv[] = { const_cast<char*>(agent_path_utf8.c_str()), nullptr };
        execv(agent_path_utf8.c_str(), argv);
        _exit(127);
    }

    return true;
}
#endif // defined(Q_OS_LINUX) || defined(Q_OS_MACOS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
bool addExplicitAce(HANDLE object, PSID sid, DWORD access, DWORD inheritance)
{
    PACL old_dacl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (GetSecurityInfo(object, SE_WINDOW_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr,
                        &old_dacl, nullptr, &descriptor) != ERROR_SUCCESS)
    {
        PLOG(ERROR) << "GetSecurityInfo failed";
        return false;
    }

    EXPLICIT_ACCESSW entry;
    memset(&entry, 0, sizeof(entry));
    entry.grfAccessPermissions = access;
    entry.grfAccessMode = GRANT_ACCESS;
    entry.grfInheritance = inheritance;
    entry.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    entry.Trustee.TrusteeType = TRUSTEE_IS_USER;
    entry.Trustee.ptstrName = reinterpret_cast<LPWSTR>(sid);

    bool result = false;

    PACL new_dacl = nullptr;
    if (SetEntriesInAclW(1, &entry, old_dacl, &new_dacl) == ERROR_SUCCESS)
    {
        result = SetSecurityInfo(object, SE_WINDOW_OBJECT, DACL_SECURITY_INFORMATION, nullptr,
                                 nullptr, new_dacl, nullptr) == ERROR_SUCCESS;
        if (new_dacl)
            LocalFree(new_dacl);
    }

    LocalFree(descriptor);
    return result;
}

//--------------------------------------------------------------------------------------------------
// Grants the target user access to the interactive window station and its default desktop. Without
// this a process launched under a user other than the one owning the console fails to initialize.
bool grantWindowStationAccess(HANDLE token)
{
    DWORD length = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &length);
    if (length == 0)
        return false;

    std::vector<char> buffer(length);
    if (!GetTokenInformation(token, TokenUser, buffer.data(), length, &length))
    {
        PLOG(ERROR) << "GetTokenInformation failed";
        return false;
    }

    PSID sid = reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid;

    HWINSTA station = OpenWindowStationW(L"winsta0", FALSE, READ_CONTROL | WRITE_DAC);
    if (!station)
    {
        PLOG(ERROR) << "OpenWindowStationW failed";
        return false;
    }

    HWINSTA previous_station = GetProcessWindowStation();
    SetProcessWindowStation(station);
    HDESK desktop = OpenDesktopW(L"default", 0, FALSE, READ_CONTROL | WRITE_DAC);
    SetProcessWindowStation(previous_station);

    bool result = true;

    // First ACE is inheritable (flows to desktops); the second one applies to the station itself.
    if (!addExplicitAce(station, sid, GENERIC_ALL,
                        CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE | OBJECT_INHERIT_ACE) ||
        !addExplicitAce(station, sid, WINSTA_ALL_ACCESS | STANDARD_RIGHTS_REQUIRED,
                        NO_PROPAGATE_INHERIT_ACE))
    {
        result = false;
    }

    if (desktop)
    {
        if (!addExplicitAce(desktop, sid, GENERIC_ALL, NO_PROPAGATE_INHERIT_ACE))
            result = false;
        CloseDesktop(desktop);
    }
    else
    {
        PLOG(ERROR) << "OpenDesktopW failed";
        result = false;
    }

    CloseWindowStation(station);
    return result;
}

//--------------------------------------------------------------------------------------------------
std::wstring createEnvironment(HANDLE token, const QString& name, const QString& value)
{
    void* environment = nullptr;
    if (!CreateEnvironmentBlock(&environment, token, FALSE))
    {
        PLOG(ERROR) << "CreateEnvironmentBlock failed";
        return {};
    }

    const wchar_t* base = reinterpret_cast<const wchar_t*>(environment);

    // Walk the entries of base (each null-terminated; block ends with an extra null).
    size_t entries_chars = 0;
    while (base[entries_chars] != L'\0')
    {
        while (base[entries_chars] != L'\0')
            ++entries_chars;
        ++entries_chars;
    }

    std::wstring result(base, entries_chars);
    result += qUtf16Printable(name + '=' + value);
    result += L'\0';

    DestroyEnvironmentBlock(environment);
    return result;
}

//--------------------------------------------------------------------------------------------------
bool startProcessWithToken(HANDLE token, const QString& command_line, const QString& channel_id)
{
    STARTUPINFOW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));

    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = const_cast<wchar_t*>(kDefaultDesktopName);

    std::wstring environment = createEnvironment(token, IpcServer::kChannelIdEnvVar, channel_id);
    if (environment.empty())
    {
        LOG(ERROR) << "Unable to create environment";
        return false;
    }

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    if (!CreateProcessAsUserW(token, nullptr, const_cast<wchar_t*>(qUtf16Printable(command_line)),
        nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW, environment.data(),
        nullptr, &startup_info, &process_info))
    {
        PLOG(ERROR) << "CreateProcessAsUserW failed";
        return false;
    }

    ScopedHandle thread_handle(process_info.hThread);
    ScopedHandle process_handle(process_info.hProcess);

    // Harden process: only SYSTEM/Administrators may terminate, suspend, inject into or patch this
    // terminal agent.
    if (!setProtectiveProcessDacl(process_info.hProcess, token))
        LOG(ERROR) << "setProtectiveProcessDacl failed";

    return true;
}
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
QString agentFilePath()
{
    QString file_path = QCoreApplication::applicationDirPath();
    file_path.append('/');
    file_path.append(kTerminalAgentFile);
    return QDir::toNativeSeparators(file_path);
}

} // namespace

//--------------------------------------------------------------------------------------------------
TerminalClient::TerminalClient(TcpChannel* tcp_channel, QObject* parent)
    : Client(tcp_channel, parent),
      attach_timer_(new QTimer(this))
{
    CLOG(INFO) << "Ctor";

    attach_timer_->setSingleShot(true);
    connect(attach_timer_, &QTimer::timeout, this, [this]()
    {
        CLOG(ERROR) << "Timeout at the start of the agent process";
        onError(FROM_HERE);
    });
}

//--------------------------------------------------------------------------------------------------
TerminalClient::~TerminalClient()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void TerminalClient::onIpcNewConnection()
{
    CLOG(INFO) << "IPC channel for terminal session is connected";

    if (!ipc_server_)
    {
        CLOG(ERROR) << "No IPC server instance!";
        onError(FROM_HERE);
        return;
    }

    if (!ipc_server_->hasPendingConnections())
    {
        CLOG(ERROR) << "No pending connections in IPC server";
        onError(FROM_HERE);
        return;
    }

    ipc_channel_ = ipc_server_->nextPendingConnection();
    ipc_channel_->setParent(this);

    const quint32 client_pid = ipc_channel_->processId();

    // Verify the connecting peer's executable is exactly the agent binary we shipped.
    const QString expected_path = QFileInfo(agentFilePath()).canonicalFilePath();
    const QString actual_path = QFileInfo(ProcessUtil::filePath(client_pid)).canonicalFilePath();
    if (actual_path.isEmpty() || actual_path != expected_path)
    {
        CLOG(ERROR) << "IPC client has unexpected executable (pid:" << client_pid
                    << "path:" << actual_path << "expected:" << expected_path << ")";
        ipc_channel_.reset();
        onError(FROM_HERE);
        return;
    }

    ipc_server_->disconnect(this);
    ipc_server_.reset();

    connect(ipc_channel_, &IpcChannel::sig_disconnected, this, &TerminalClient::onIpcDisconnected);
    connect(ipc_channel_, &IpcChannel::sig_messageReceived, this, &TerminalClient::onIpcMessageReceived);

    attach_timer_->stop();
    ipc_channel_->setPaused(false);

    sendResult(proto::terminal::Result::CODE_SUCCESS);

    // Flush messages that arrived while the agent was starting up (e.g. the initial resize).
    for (const QByteArray& message : std::as_const(pending_to_agent_))
        ipc_channel_->send(0, message);
    pending_to_agent_.clear();

    CLOG(INFO) << "Terminal agent is connected";
}

//--------------------------------------------------------------------------------------------------
void TerminalClient::onIpcErrorOccurred()
{
    onError(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void TerminalClient::onIpcMessageReceived(
    quint32 /* ipc_channel_id */, const QByteArray& buffer, bool /* reliable */)
{
    send(proto::peer::CHANNEL_ID_0, buffer);
}

//--------------------------------------------------------------------------------------------------
void TerminalClient::onIpcDisconnected()
{
    onError(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void TerminalClient::onStart()
{
    // The session is established. The agent process is launched only after the client supplies the
    // operating system credentials (see onMessage).
    emit sig_started();
}

//--------------------------------------------------------------------------------------------------
void TerminalClient::onMessage(quint8 /* tcp_channel_id */, const QByteArray& buffer)
{
    if (!agent_launched_)
    {
        proto::terminal::ClientToHost message;
        if (!parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse message";
            return;
        }

        if (!message.has_credentials())
        {
            // Input or resize messages may arrive before authentication completes (the client sends
            // the initial resize right after the credentials). Ignore them until the agent starts.
            return;
        }

        const proto::terminal::Credentials& credentials = message.credentials();

        QString ipc_channel_id = IpcServer::createUniqueId();
        if (!launchAgent(QString::fromStdString(credentials.user_name()),
                         QString::fromStdString(credentials.password()), ipc_channel_id))
        {
            ++auth_failure_count_;
            CLOG(WARNING) << "Authentication failed (attempt" << auth_failure_count_ << "of"
                          << kMaxAuthFailures << ")";

            sendResult(proto::terminal::Result::CODE_AUTHENTICATION_ERROR);
            ipc_server_.reset();

            // Allow another attempt until the limit is reached, then close the session.
            if (auth_failure_count_ >= kMaxAuthFailures)
            {
                CLOG(WARNING) << "Maximum authentication attempts reached, closing session";
                onError(FROM_HERE);
            }
            return;
        }

        agent_launched_ = true;

        CLOG(INFO) << "Wait for starting agent process";
        attach_timer_->start(std::chrono::seconds(10));
        return;
    }

    if (!ipc_channel_)
    {
        // The agent is still starting up; queue the message until its IPC channel connects.
        pending_to_agent_.append(buffer);
        return;
    }

    ipc_channel_->send(0, buffer);
}

//--------------------------------------------------------------------------------------------------
bool TerminalClient::launchAgent(
    const QString& user_name, const QString& password, const QString& ipc_channel_id)
{
    if (user_name.isEmpty())
    {
        CLOG(ERROR) << "Empty user name";
        return false;
    }

#if defined(Q_OS_WINDOWS)
    QString domain = ".";
    QString account = user_name;

    const int separator = user_name.indexOf('\\');
    if (separator != -1)
    {
        domain = user_name.left(separator);
        account = user_name.mid(separator + 1);
    }

    ScopedHandle user_token;
    if (!LogonUserW(qUtf16Printable(account), qUtf16Printable(domain), qUtf16Printable(password),
        LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, user_token.recieve()))
    {
        PLOG(WARNING) << "LogonUserW failed";
        return false;
    }

    QString target_user_sid;
    if (!tokenUserSidString(user_token, &target_user_sid))
    {
        CLOG(ERROR) << "Failed to query target user SID from token";
        return false;
    }

    if (!startIpcServer(ipc_channel_id, target_user_sid))
    {
        CLOG(ERROR) << "Unable to start IPC server";
        return false;
    }

    if (!grantWindowStationAccess(user_token))
        CLOG(WARNING) << "Unable to grant window station access; the agent may fail to start";

    CLOG(INFO) << "Starting terminal agent process";
    if (!startProcessWithToken(user_token, agentFilePath(), ipc_channel_id))
    {
        CLOG(ERROR) << "startProcessWithToken failed";
        return false;
    }

    return true;
#elif defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    if (!pamAuthenticate(user_name, password))
        return false;

    if (!startIpcServer(ipc_channel_id, QString()))
    {
        CLOG(ERROR) << "Unable to start IPC server";
        return false;
    }

    CLOG(INFO) << "Starting terminal agent process for user:" << user_name;
    if (!launchAgentAsUser(user_name, agentFilePath(), ipc_channel_id))
    {
        CLOG(ERROR) << "Unable to launch agent process";
        return false;
    }

    return true;
#else
    NOTIMPLEMENTED();
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
bool TerminalClient::startIpcServer(const QString& ipc_channel_name, const QString& target_user_sid)
{
    CLOG(INFO) << "Starting IPC server for terminal session";

    if (ipc_server_)
    {
        CLOG(ERROR) << "IPC server is already started";
        return false;
    }

    ipc_server_ = new IpcServer(this);

    connect(ipc_server_, &IpcServer::sig_newConnection, this, &TerminalClient::onIpcNewConnection);
    connect(ipc_server_, &IpcServer::sig_errorOccurred, this, &TerminalClient::onIpcErrorOccurred);

    // The agent runs under the target user's account. On Windows restrict the pipe DACL to that SID;
    // on UNIX the SID is empty and the default mode is used.
    const IpcServer::AccessMode mode = target_user_sid.isEmpty() ?
        IpcServer::AccessMode::INTERACTIVE_USER : IpcServer::AccessMode::SPECIFIC_USER;

    if (!ipc_server_->start(ipc_channel_name, mode, target_user_sid))
    {
        CLOG(ERROR) << "Failed to start IPC server:" << ipc_channel_name;
        return false;
    }

    CLOG(INFO) << "IPC server is started";
    return true;
}

//--------------------------------------------------------------------------------------------------
void TerminalClient::sendResult(proto::terminal::Result::Code code)
{
    proto::terminal::HostToClient message;
    message.mutable_result()->set_code(code);
    send(proto::peer::CHANNEL_ID_0, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void TerminalClient::onError(const Location& location)
{
    if (isFinished())
        return;

    CLOG(ERROR) << "Error occurred (" << location << ")";

    attach_timer_->stop();

    if (ipc_server_)
        ipc_server_->disconnect(this);

    if (ipc_channel_)
        ipc_channel_->disconnect(this);

    finish();
}
