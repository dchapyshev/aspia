//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/single_application.h"
#include "base/qt_logging.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/scoped_object.h"
#endif // defined(OS_WIN)

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QThread>

#if defined(OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#endif // defined(OS_WIN)

namespace base {

namespace {

const int kMaxPendingConnections = 25;
const int kConnectTimeoutMs = 1000;
const int kDisconnectTimeoutMs = 1000;
const int kReconnectIntervalMs = 500;
const int kReconnectTryCount = 3;
const int kReadTimeoutMs = 1500;
const int kWriteTimeoutMs = 1500;
const int kMaxMessageSize = 1024 * 1024 * 1;

const char kOkMessage[] = "OK";

#if defined(OS_WIN)
bool isSameApplication(const QLocalSocket* socket)
{
    ULONG process_id;

    if (!GetNamedPipeClientProcessId(
        reinterpret_cast<HANDLE>(socket->socketDescriptor()), &process_id))
    {
        PLOG(LS_ERROR) << "GetNamedPipeClientProcessId failed";
        return false;
    }

    win::ScopedHandle other_process(
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id));
    if (!other_process.isValid())
    {
        PLOG(LS_ERROR) << "OpenProcess failed";
        return false;
    }

    wchar_t other_path[MAX_PATH];
    DWORD size = MAX_PATH;

    if (!QueryFullProcessImageNameW(other_process, 0, other_path, &size))
    {
        PLOG(LS_ERROR) << "QueryFullProcessImageNameW failed";
        return false;
    }

    wchar_t current_path[MAX_PATH];
    size = MAX_PATH;

    if (!QueryFullProcessImageNameW(GetCurrentProcess(), 0, current_path, &size))
    {
        PLOG(LS_ERROR) << "QueryFullProcessImageNameW failed";
        return false;
    }

    return _wcsicmp(current_path, other_path) == 0;
}
#endif // defined(OS_WIN)

} // namespace

SingleApplication::SingleApplication(int& argc, char* argv[])
    : QApplication(argc, argv)
{
#if defined(OS_WIN)
    DWORD id = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &id);
    QString session_id = QString::number(id);
#else
    QString session_id = QString::number(getuid());
#endif

    QString temp_path = QDir::tempPath();

    if (temp_path.endsWith(QLatin1Char('\\')) || temp_path.endsWith(QLatin1Char('/')))
        temp_path = temp_path.left(temp_path.length() - 1);

    if (temp_path.endsWith(session_id))
        temp_path = temp_path.left(temp_path.length() - session_id.length());

    QByteArray app_path = QApplication::applicationFilePath().toLower().toUtf8();
    QByteArray app_path_hash = QCryptographicHash::hash(app_path, QCryptographicHash::Md5);

    server_name_ = QString::fromLatin1(app_path_hash.toHex()) + session_id;
    lock_file_name_ = temp_path + QLatin1Char('/') + server_name_ + QStringLiteral(".lock");
    lock_file_ = new QLockFile(lock_file_name_);
}

SingleApplication::~SingleApplication()
{
    bool is_locked = lock_file_->isLocked();

    if (is_locked)
        lock_file_->unlock();

    delete lock_file_;

    if (is_locked)
        QFile::remove(lock_file_name_);
}

bool SingleApplication::isRunning()
{
    if (!lock_file_->tryLock())
        return true;

    server_ = new QLocalServer(this);

    server_->setSocketOptions(QLocalServer::UserAccessOption);
    server_->setMaxPendingConnections(kMaxPendingConnections);

    if (!server_->listen(server_name_))
    {
        LOG(LS_ERROR) << "IPC server is not running on channel "
                      << server_->serverName() << ": " << server_->errorString();
    }
    else
    {
        connect(server_, &QLocalServer::newConnection, this, &SingleApplication::onNewConnection);
    }

    return false;
}

void SingleApplication::sendMessage(const QByteArray& message)
{
    if (server_)
    {
        DLOG(LS_ERROR) << "Attempt to send a message from the first instance of the application";
        return;
    }

    QLocalSocket socket;

    for (int i = 0; i < kReconnectTryCount; ++i)
    {
        socket.connectToServer(server_name_);

        if (socket.waitForConnected(kConnectTimeoutMs))
            break;

        if (i == kReconnectTryCount - 1)
            break;

        QThread::msleep(kReconnectIntervalMs);
    }

    if (socket.state() != QLocalSocket::LocalSocketState::ConnectedState)
    {
        LOG(LS_ERROR) << "Could not connect to server";
        return;
    }

    QDataStream stream(&socket);
    stream.writeBytes(message.constData(), message.size());

    if (!socket.waitForBytesWritten(kWriteTimeoutMs))
    {
        LOG(LS_ERROR) << "Timeout when sending a message";
        return;
    }

    if (!socket.waitForReadyRead(kReadTimeoutMs))
    {
        LOG(LS_ERROR) << "Timeout when reading a message";
        return;
    }

    if (socket.read(strlen(kOkMessage)) != kOkMessage)
    {
        LOG(LS_ERROR) << "Unknown status code";
        return;
    }
}

void SingleApplication::onNewConnection()
{
    DCHECK(server_);

    std::unique_ptr<QLocalSocket> socket(server_->nextPendingConnection());
    if (!socket)
    {
        LOG(LS_ERROR) << "Invalid socket";
        return;
    }

#if defined(OS_WIN)
    if (!isSameApplication(socket.get()))
    {
        LOG(LS_ERROR) << "Attempt to connect from unknown application";
        return;
    }
#endif // defined(OS_WIN)

    while (true)
    {
        if (socket->state() == QLocalSocket::UnconnectedState)
            return;

        if (socket->bytesAvailable() >= sizeof(uint32_t))
            break;

        socket->waitForReadyRead();
    }

    QDataStream stream(socket.get());
    uint32_t remaining;

    stream >> remaining;

    if (!remaining || remaining > kMaxMessageSize)
    {
        LOG(LS_ERROR) << "Message has an incorrect size: " << remaining;
        return;
    }

    QByteArray message;
    message.resize(remaining);

    char* buffer = message.data();

    do
    {
        int read_bytes = stream.readRawData(buffer, remaining);
        if (read_bytes < 0)
        {
            LOG(LS_ERROR) << "Could not read message";
            return;
        }

        buffer += read_bytes;
        remaining -= read_bytes;
    }
    while (remaining && socket->waitForReadyRead(kReadTimeoutMs));

    socket->write(kOkMessage, strlen(kOkMessage));
    socket->waitForBytesWritten(kWriteTimeoutMs);
    socket->waitForDisconnected(kDisconnectTimeoutMs);

    emit messageReceived(message);
}

} // namespace base
