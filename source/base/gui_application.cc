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

#include "base/gui_application.h"

#include <QDataStream>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>

#include "base/logging.h"
#include "base/crypto/scoped_crypto_initializer.h"

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_UNIX)
#include <unistd.h>
#endif // defined(Q_OS_UNIX)

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

} // namespace

//--------------------------------------------------------------------------------------------------
GuiApplication::GuiApplication(int& argc, char* argv[])
    : QApplication(argc, argv),
      io_thread_(base::Thread::AsioDispatcher, nullptr)
{
    LOG(INFO) << "Ctor";

#if defined(Q_OS_WINDOWS)
    DWORD id = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &id))
    {
        PLOG(ERROR) << "ProcessIdToSessionId failed";
    }
    QString session_id = QString::number(id);
#else
    QString session_id = QString::number(getuid());
#endif

    QString temp_path = QDir::tempPath();

    if (temp_path.endsWith(QLatin1Char('\\')) || temp_path.endsWith(QLatin1Char('/')))
        temp_path = temp_path.left(temp_path.length() - 1);

    if (temp_path.endsWith(session_id))
        temp_path = temp_path.left(temp_path.length() - session_id.length());

    QByteArray app_path_hex = QApplication::applicationFilePath().toLower().toUtf8().toHex();

    server_name_ = QString::fromLatin1(app_path_hex) + session_id;
    lock_file_name_ = temp_path + QLatin1Char('/') + server_name_ + QLatin1String(".lock");
    lock_file_ = new QLockFile(lock_file_name_);

    crypto_initializer_ = std::make_unique<base::ScopedCryptoInitializer>();
    CHECK(crypto_initializer_->isSucceeded());

    io_thread_.start();

    translations_ = std::make_unique<Translations>();
}

//--------------------------------------------------------------------------------------------------
GuiApplication::~GuiApplication()
{
    LOG(INFO) << "Dtor";

    io_thread_.stop();

    bool is_locked = lock_file_->isLocked();

    if (is_locked)
        lock_file_->unlock();

    delete lock_file_;

    if (is_locked)
        QFile::remove(lock_file_name_);
}

//--------------------------------------------------------------------------------------------------
// static
GuiApplication* GuiApplication::instance()
{
    return static_cast<GuiApplication*>(QApplication::instance());
}

//--------------------------------------------------------------------------------------------------
// static
QThread* GuiApplication::ioThread()
{
    GuiApplication* application = instance();
    if (!application)
    {
        LOG(ERROR) << "Invalid application instance";
        return nullptr;
    }

    return &application->io_thread_;
}

//--------------------------------------------------------------------------------------------------
bool GuiApplication::isRunning()
{
    if (!lock_file_->tryLock())
    {
        LOG(INFO) << "Application already running";
        return true;
    }

    server_ = new QLocalServer(this);

    server_->setSocketOptions(QLocalServer::UserAccessOption);
    server_->setMaxPendingConnections(kMaxPendingConnections);

    if (!server_->listen(server_name_))
    {
        LOG(ERROR) << "IPC server is not running on channel"
                   << server_->serverName() << ":" << server_->errorString();
    }
    else
    {
        connect(server_, &QLocalServer::newConnection, this, &GuiApplication::onNewConnection);
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
GuiApplication::LocaleList GuiApplication::localeList() const
{
    return translations_->localeList();
}

//--------------------------------------------------------------------------------------------------
void GuiApplication::setLocale(const QString& locale)
{
    translations_->installTranslators(locale);
}

//--------------------------------------------------------------------------------------------------
bool GuiApplication::hasLocale(const QString& locale)
{
    return translations_->contains(locale);
}

//--------------------------------------------------------------------------------------------------
void GuiApplication::sendMessage(const QByteArray& message)
{
    if (server_)
    {
        LOG(ERROR) << "Attempt to send a message from the first instance of the application";
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
        LOG(ERROR) << "Could not connect to server";
        return;
    }

    QDataStream stream(&socket);
    stream.writeBytes(message.constData(), static_cast<uint>(message.size()));

    if (!socket.waitForBytesWritten(kWriteTimeoutMs))
    {
        LOG(ERROR) << "Timeout when sending a message";
        return;
    }

    if (!socket.waitForReadyRead(kReadTimeoutMs))
    {
        LOG(ERROR) << "Timeout when reading a message";
        return;
    }

    if (socket.read(strlen(kOkMessage)) != kOkMessage)
    {
        LOG(ERROR) << "Unknown status code";
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void GuiApplication::onNewConnection()
{
    DCHECK(server_);

    std::unique_ptr<QLocalSocket> socket(server_->nextPendingConnection());
    if (!socket)
    {
        LOG(ERROR) << "Invalid socket";
        return;
    }

    while (true)
    {
        if (socket->state() == QLocalSocket::UnconnectedState)
            return;

        if (socket->bytesAvailable() >= static_cast<int>(sizeof(quint32)))
            break;

        socket->waitForReadyRead();
    }

    QDataStream stream(socket.get());
    quint32 remaining;

    stream >> remaining;

    if (!remaining || remaining > kMaxMessageSize)
    {
        LOG(ERROR) << "Message has an incorrect size:" << remaining;
        return;
    }

    QByteArray message;
    message.resize(static_cast<int>(remaining));

    char* buffer = message.data();

    do
    {
        int read_bytes = stream.readRawData(buffer, static_cast<int>(remaining));
        if (read_bytes < 0)
        {
            LOG(ERROR) << "Could not read message";
            return;
        }

        buffer += read_bytes;
        remaining -= static_cast<quint32>(read_bytes);
    }
    while (remaining && socket->waitForReadyRead(kReadTimeoutMs));

    socket->write(kOkMessage, strlen(kOkMessage));
    socket->waitForBytesWritten(kWriteTimeoutMs);
    socket->waitForDisconnected(kDisconnectTimeoutMs);

    emit sig_messageReceived(message);
}

} // namespace base
