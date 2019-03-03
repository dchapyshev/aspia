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
const int kConnectTimeoutMs = 500;
const int kReconnectIntervalMs = 500;
const int kReconnectTryCount = 3;
const int kReadTimeoutMs = 500;
const int kWriteTimeoutMs = 500;
const int kMaxMessageSize = 1024 * 1024 * 1;

QString createServerName()
{
    QByteArray path_hash = QCryptographicHash::hash(
        QApplication::applicationFilePath().toLower().toUtf8(), QCryptographicHash::Md5);

    QString user_session;

#if defined(OS_WIN)
    DWORD session_id = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session_id);
    user_session = QString::number(session_id, 16);
#else
    user_session = QString::number(getuid(), 16);
#endif

    return QString::fromLatin1(path_hash.toHex()) + user_session;
}

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
        OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process_id));
    if (!other_process.isValid())
    {
        PLOG(LS_ERROR) << "OpenProcess failed";
        return false;
    }

    wchar_t other_path[MAX_PATH];

    if (!GetModuleFileNameExW(other_process, nullptr, other_path, _countof(other_path)))
    {
        PLOG(LS_ERROR) << "GetModuleFileNameExW failed";
        return false;
    }

    wchar_t current_path[MAX_PATH];

    if (!GetModuleFileNameExW(GetCurrentProcess(), nullptr, current_path, _countof(current_path)))
    {
        PLOG(LS_ERROR) << "GetModuleFileNameExW failed";
        return false;
    }

    return _wcsicmp(current_path, other_path) == 0;
}
#endif // defined(OS_WIN)

} // namespace

SingleApplication::SingleApplication(int& argc, char* argv[])
    : QApplication(argc, argv),
      server_name_(createServerName())
{
    lock_file_name_ = QDir::tempPath() + QLatin1Char('/') + server_name_ + QStringLiteral(".lock");
    lock_file_ = new QLockFile(lock_file_name_);
}

SingleApplication::~SingleApplication()
{
    if (lock_file_->isLocked())
    {
        lock_file_->unlock();
        QFile::remove(lock_file_name_);
    }
}

bool SingleApplication::isRunning()
{
    if (!lock_file_->tryLock())
    {
        std::unique_ptr<QLocalSocket> socket = std::make_unique<QLocalSocket>(this);

        for (int i = 0; i < kReconnectTryCount; ++i)
        {
            socket->connectToServer(server_name_);

            if (socket->waitForConnected(kConnectTimeoutMs))
                break;

            if (i == kReconnectTryCount - 1)
                break;

            QThread::msleep(kReconnectIntervalMs);
        }

        if (socket->isOpen())
        {
            socket_ = socket.release();
            return true;
        }
    }
    else
    {
        std::unique_ptr<QLocalServer> server = std::make_unique<QLocalServer>(this);

        server->setSocketOptions(QLocalServer::UserAccessOption);
        server->setMaxPendingConnections(kMaxPendingConnections);

        if (!server->listen(server_name_))
        {
            LOG(LS_ERROR) << "IPC server is not running on channel "
                << server->serverName() << ": " << server->errorString();
            return false;
        }

        server_ = server.release();

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

    DCHECK(socket_);

    QDataStream stream(socket_);
    stream.writeBytes(message.constData(), message.size());

    if (!socket_->waitForBytesWritten(kWriteTimeoutMs))
    {
        LOG(LS_ERROR) << "Timeout when sending a message";
        return;
    }
}

void SingleApplication::onNewConnection()
{
    DCHECK(server_);

    std::unique_ptr<QLocalSocket> socket(server_->nextPendingConnection());
    if (!socket)
        return;

#if defined(OS_WIN)
    if (!isSameApplication(socket.get()))
    {
        LOG(LS_ERROR) << "Attempt to connect from unknown application.";
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

    emit messageReceived(message);
}

} // namespace base
