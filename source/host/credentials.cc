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

#include "host/credentials.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>

#include <qt_windows.h>
#include <strsafe.h>

#include "base/logging.h"
#include "base/process_util.h"
#include "base/crypto/secure_memory.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "base/win/scoped_object.h"
#include "base/win/security_helpers.h"

namespace {

const char kChannelId[] = "org.aspia.host.credentials";
const int kFieldChars = 256;

struct Reply
{
    wchar_t domain[kFieldChars];
    wchar_t username[kFieldChars];
    wchar_t password[kFieldChars];
};

struct Request
{
    quint8 screen_type;
    quint8 reason;
    quint8 reserved[2];
};

//--------------------------------------------------------------------------------------------------
bool isValidAccountString(const QString& str)
{
    static const QString kInvalidChars("\"/\\[]:;|=,+*?<>");

    for (const QChar c : str)
    {
        if (c.unicode() < 0x20 || kInvalidChars.contains(c))
            return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool isSystemProcess(quint32 pid)
{
    ScopedHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!process.isValid())
    {
        PLOG(ERROR) << "OpenProcess failed";
        return false;
    }

    ScopedHandle token;
    if (!OpenProcessToken(process.get(), TOKEN_QUERY, token.recieve()))
    {
        PLOG(ERROR) << "OpenProcessToken failed";
        return false;
    }

    QString user_sid;
    if (!tokenUserSidString(token.get(), &user_sid))
    {
        LOG(ERROR) << "Failed to query the token user SID";
        return false;
    }

    return user_sid == "S-1-5-18";
}

//--------------------------------------------------------------------------------------------------
bool isAllowedHostProcess(quint32 pid)
{
    wchar_t system_dir[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(system_dir, ARRAYSIZE(system_dir));
    if (length == 0 || length >= ARRAYSIZE(system_dir))
    {
        PLOG(ERROR) << "GetSystemDirectoryW failed";
        return false;
    }

    const QString actual = QFileInfo(ProcessUtil::filePath(pid)).canonicalFilePath();
    if (actual.isEmpty())
        return false;

    const QDir system_directory(QString::fromWCharArray(system_dir, length));
    static const char* const kAllowedNames[] = { "LogonUI.exe", "consent.exe" };

    for (const char* name : kAllowedNames)
    {
        const QString expected = QFileInfo(system_directory.filePath(name)).canonicalFilePath();
        if (!expected.isEmpty() && actual.compare(expected, Qt::CaseInsensitive) == 0)
            return true;
    }

    return false;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Credentials::Credentials(QObject* parent)
    : QObject(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
Credentials::~Credentials()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool Credentials::start()
{
    CHECK(!ipc_server_);

    if (qEnvironmentVariableIsSet("ASPIA_DISABLE_CREDENTIALS"))
    {
        LOG(INFO) << "Disabled via ASPIA_DISABLE_CREDENTIALS";
        return false;
    }

    ipc_server_ = new IpcServer(this);

    connect(ipc_server_, &IpcServer::sig_newConnection, this, &Credentials::onIpcNewConnection);
    connect(ipc_server_, &IpcServer::sig_errorOccurred, this, &Credentials::onIpcErrorOccurred);

    if (!ipc_server_->start(kChannelId, IpcServer::AccessMode::SYSTEM_ONLY))
    {
        LOG(ERROR) << "Failed to start the credentials IPC server";
        return false;
    }

    LOG(INFO) << "Credentials IPC server started on channel:" << kChannelId;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Credentials::sendCredentials(const SecureString& username, const SecureString& password)
{
    if (!ipc_channel_)
    {
        LOG(WARNING) << "No credential provider connected";
        return false;
    }

    const QString& full_name = username.toString();
    if (full_name.size() >= 2 * kFieldChars)
    {
        LOG(WARNING) << "User name is too long";
        return false;
    }

    QString domain;
    QString user = full_name;

    const qsizetype separator = full_name.indexOf('\\');
    if (separator >= 0)
    {
        domain = full_name.left(separator);
        user = full_name.mid(separator + 1);
    }

    if (user.isEmpty() || user.size() >= kFieldChars || !isValidAccountString(user))
    {
        LOG(WARNING) << "Invalid user name";
        return false;
    }

    if (password.isEmpty() || password.size() >= kFieldChars)
    {
        LOG(WARNING) << "Invalid password";
        return false;
    }

    if (domain.size() >= kFieldChars || !isValidAccountString(domain))
    {
        LOG(WARNING) << "Invalid domain";
        return false;
    }

    Reply reply = {};
    StringCchCopyW(reply.domain, kFieldChars, qUtf16Printable(domain));
    StringCchCopyW(reply.username, kFieldChars, qUtf16Printable(user));
    StringCchCopyW(reply.password, kFieldChars, qUtf16Printable(password.toString()));

    memZero(&domain);
    memZero(&user);

    ipc_channel_->send(0, QByteArray(reinterpret_cast<const char*>(&reply),
                                     static_cast<qsizetype>(sizeof(reply))),
                       /* reliable */ true, /* secure */ true);

    memZero(&reply, sizeof(reply));

    LOG(INFO) << "Credentials sent to the credential provider";
    return true;
}

//--------------------------------------------------------------------------------------------------
void Credentials::onIpcNewConnection()
{
    CHECK(ipc_server_);

    if (!ipc_server_->hasPendingConnections())
    {
        LOG(ERROR) << "No pending IPC connections";
        return;
    }

    ScopedQPointer<IpcChannel> channel(ipc_server_->nextPendingConnection());
    CHECK(channel);

    const quint32 client_pid = channel->processId();
    if (!isSystemProcess(client_pid))
    {
        LOG(ERROR) << "Rejecting non-SYSTEM IPC client (pid:" << client_pid << ")";
        return;
    }

    if (!isAllowedHostProcess(client_pid))
    {
        LOG(ERROR) << "Rejecting IPC client that is not an allowed host process (pid:" << client_pid << ")";
        return;
    }

    if (ipc_channel_)
    {
        LOG(INFO) << "Superseding the previously connected credential provider";
        ipc_channel_->disconnect();
        ipc_channel_.reset();
    }

    channel->setParent(this);
    ipc_channel_ = std::move(channel);

    LOG(INFO) << "Credential provider connected (pid:" << ipc_channel_->processId()
              << "session:" << ipc_channel_->sessionId() << ")";

    connect(ipc_channel_, &IpcChannel::sig_messageReceived, this, &Credentials::onIpcMessageReceived);
    connect(ipc_channel_, &IpcChannel::sig_disconnected, this, &Credentials::onIpcDisconnected);

    ipc_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
void Credentials::onIpcErrorOccurred()
{
    LOG(WARNING) << "Credentials IPC server error";
}

//--------------------------------------------------------------------------------------------------
void Credentials::onIpcMessageReceived(quint32 /* channel_id */, const QByteArray& buffer, bool /* reliable */)
{
    if (buffer.size() != sizeof(Request))
    {
        LOG(WARNING) << "Unexpected request size:" << buffer.size();
        return;
    }

    const Request* request = reinterpret_cast<const Request*>(buffer.constData());

    LOG(INFO) << "Credentials requested by the credential provider (screen_type:" << request->screen_type
              << "reason:" << request->reason << ")";
    emit sig_connected(request->screen_type, request->reason);
}

//--------------------------------------------------------------------------------------------------
void Credentials::onIpcDisconnected()
{
    LOG(INFO) << "Credential provider disconnected";
    if (ipc_channel_)
        ipc_channel_->disconnect(this);
    ipc_channel_.reset();
}
