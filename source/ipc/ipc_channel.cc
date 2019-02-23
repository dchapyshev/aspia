//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "ipc/ipc_channel.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // defined(OS_WIN)

#include "base/qt_logging.h"

namespace ipc {

Q_DECLARE_METATYPE(QLocalSocket::LocalSocketError);

namespace {

constexpr uint32_t kMaxMessageSize = 16 * 1024 * 1024; // 16MB

} // namespace

Channel::Channel(QLocalSocket* socket, QObject* parent)
    : QObject(parent),
      socket_(socket)
{
    DCHECK(socket_);

    qRegisterMetaType<QLocalSocket::LocalSocketError>();

    socket_->setParent(this);

    connect(socket_, &QLocalSocket::connected, this, &Channel::connected);
    connect(socket_, &QLocalSocket::bytesWritten, this, &Channel::onBytesWritten);
    connect(socket_, &QLocalSocket::readyRead, this, &Channel::onReadyRead);
    connect(socket_, &QLocalSocket::disconnected, this, &Channel::disconnected);
    connect(socket_, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error),
            this, &Channel::onError);
}

// static
Channel* Channel::createClient(QObject* parent)
{
    return new Channel(new QLocalSocket(), parent);
}

void Channel::connectToServer(const QString& channel_name)
{
    socket_->connectToServer(channel_name);
}

base::ProcessId Channel::clientProcessId() const
{
#if defined(OS_WIN)
    ULONG process_id = base::kNullProcessId;

    if (!GetNamedPipeClientProcessId(reinterpret_cast<HANDLE>(socket_->socketDescriptor()),
                                     &process_id))
    {
        PLOG(LS_WARNING) << "GetNamedPipeClientProcessId failed";
        return base::kNullProcessId;
    }

    return process_id;
#else
#error Platform support not implemented
#endif
}

base::ProcessId Channel::serverProcessId() const
{
#if defined(OS_WIN)
    ULONG process_id = base::kNullProcessId;

    if (!GetNamedPipeServerProcessId(reinterpret_cast<HANDLE>(socket_->socketDescriptor()),
                                     &process_id))
    {
        PLOG(LS_WARNING) << "GetNamedPipeServerProcessId failed";
        return base::kNullProcessId;
    }

    return process_id;
#else
#error Platform support not implemented
#endif
}

#if defined(OS_WIN)
base::win::SessionId Channel::clientSessionId() const
{
    ULONG session_id = base::win::kInvalidSessionId;

    if (!GetNamedPipeClientSessionId(reinterpret_cast<HANDLE>(socket_->socketDescriptor()),
                                     &session_id))
    {
        PLOG(LS_WARNING) << "GetNamedPipeClientSessionId failed";
        return base::win::kInvalidSessionId;
    }

    return session_id;
}

base::win::SessionId Channel::serverSessionId() const
{
    ULONG session_id = base::win::kInvalidSessionId;

    if (!GetNamedPipeServerSessionId(reinterpret_cast<HANDLE>(socket_->socketDescriptor()),
                                     &session_id))
    {
        PLOG(LS_WARNING) << "GetNamedPipeServerSessionId failed";
        return base::win::kInvalidSessionId;
    }

    return session_id;
}
#endif // defined(OS_WIN)

void Channel::stop()
{
    if (socket_->state() != QLocalSocket::UnconnectedState)
    {
        socket_->abort();

        if (socket_->state() != QLocalSocket::UnconnectedState)
            socket_->waitForDisconnected();
    }
}

void Channel::start()
{
    onReadyRead();
}

void Channel::send(const QByteArray& buffer)
{
    bool schedule_write = write_queue_.isEmpty();

    write_queue_.push_back(buffer);

    if (schedule_write)
        scheduleWrite();
}

void Channel::onError(QLocalSocket::LocalSocketError /* socket_error */)
{
    LOG(LS_WARNING) << "IPC channel error: " << socket_->errorString();
    emit errorOccurred();
}

void Channel::onBytesWritten(int64_t bytes)
{
    const QByteArray& write_buffer = write_queue_.front();

    written_ += bytes;

    if (written_ < sizeof(MessageSizeType))
    {
        socket_->write(reinterpret_cast<const char*>(&write_size_) + written_,
                       sizeof(MessageSizeType) - written_);
    }
    else if (written_ < sizeof(MessageSizeType) + write_buffer.size())
    {
        socket_->write(write_buffer.data() + (written_ - sizeof(MessageSizeType)),
                       write_buffer.size() - (written_ - sizeof(MessageSizeType)));
    }
    else
    {
        write_queue_.pop_front();
        written_ = 0;

        if (!write_queue_.empty())
            scheduleWrite();
    }
}

void Channel::onReadyRead()
{
    int64_t current;

    for (;;)
    {
        if (!read_size_received_)
        {
            current = socket_->read(reinterpret_cast<char*>(&read_size_) + read_,
                                    sizeof(MessageSizeType) - read_);
            if (current + read_ == sizeof(MessageSizeType))
            {
                read_size_received_ = true;

                if (!read_size_ || read_size_ > kMaxMessageSize)
                {
                    LOG(LS_WARNING) << "Wrong message size: " << read_size_;
                    socket_->abort();
                    return;
                }

                if (read_buffer_.capacity() < static_cast<int>(read_size_))
                    read_buffer_.reserve(read_size_);

                read_buffer_.resize(read_size_);
                read_ = 0;
                continue;
            }
        }
        else if (read_ < read_size_)
        {
            current = socket_->read(read_buffer_.data() + read_, read_size_ - read_);
        }
        else
        {
            read_size_received_ = false;
            read_ = 0;

            emit messageReceived(read_buffer_);
            continue;
        }

        if (current <= 0)
            break;

        read_ += current;
    }
}

void Channel::scheduleWrite()
{
    const QByteArray& write_buffer = write_queue_.front();

    write_size_ = write_buffer.size();
    if (!write_size_ || write_size_ > kMaxMessageSize)
    {
        LOG(LS_WARNING) << "Wrong message size: " << write_size_;
        socket_->abort();
        return;
    }

    socket_->write(reinterpret_cast<const char*>(&write_size_), sizeof(MessageSizeType));
}

} // namespace ipc
