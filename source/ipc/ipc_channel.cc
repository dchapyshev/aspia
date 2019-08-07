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

#include "ipc/ipc_channel.h"

#include "build/build_config.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "qt_base/qt_logging.h"

#if defined(OS_WIN)
#include <Windows.h>
#endif // defined(OS_WIN)

namespace ipc {

Q_DECLARE_METATYPE(QLocalSocket::LocalSocketError);

namespace {

constexpr uint32_t kMaxMessageSize = 16 * 1024 * 1024; // 16MB

#if defined(OS_WIN)
base::ProcessId clientProcessIdImpl(HANDLE pipe_handle)
{
    ULONG process_id = base::kNullProcessId;

    if (!GetNamedPipeClientProcessId(pipe_handle, &process_id))
    {
        PLOG(LS_WARNING) << "GetNamedPipeClientProcessId failed";
        return base::kNullProcessId;
    }

    return process_id;
}

base::ProcessId serverProcessIdImpl(HANDLE pipe_handle)
{
    ULONG process_id = base::kNullProcessId;

    if (!GetNamedPipeServerProcessId(pipe_handle, &process_id))
    {
        PLOG(LS_WARNING) << "GetNamedPipeServerProcessId failed";
        return base::kNullProcessId;
    }

    return process_id;
}

base::win::SessionId clientSessionIdImpl(HANDLE pipe_handle)
{
    ULONG session_id = base::win::kInvalidSessionId;

    if (!GetNamedPipeClientSessionId(pipe_handle, &session_id))
    {
        PLOG(LS_WARNING) << "GetNamedPipeClientSessionId failed";
        return base::win::kInvalidSessionId;
    }

    return session_id;
}

base::win::SessionId serverSessionIdImpl(HANDLE pipe_handle)
{
    ULONG session_id = base::win::kInvalidSessionId;

    if (!GetNamedPipeServerSessionId(pipe_handle, &session_id))
    {
        PLOG(LS_WARNING) << "GetNamedPipeServerSessionId failed";
        return base::win::kInvalidSessionId;
    }

    return session_id;
}
#endif // defined(OS_WIN)

} // namespace

Channel::Channel()
    : socket_(new QLocalSocket(this)),
      proxy_(new ChannelProxy(this))
{
    // Nothing
}

Channel::Channel(QLocalSocket* socket)
    : socket_(socket),
      proxy_(new ChannelProxy(this)),
      is_connected_(true)
{
    DCHECK(socket_);

    socket_->setParent(this);
    init();

#if defined(OS_WIN)
    HANDLE pipe_handle = reinterpret_cast<HANDLE>(socket_->socketDescriptor());
    peer_process_id_ = clientProcessIdImpl(pipe_handle);
    peer_session_id_ = clientSessionIdImpl(pipe_handle);
#endif // defined(OS_WIN)
}

Channel::~Channel()
{
    if (!is_connected_)
        return;

    is_connected_ = false;

    proxy_->willDestroyCurrentChannel();
    proxy_ = nullptr;

    socket_->abort();
    socket_->waitForDisconnected();
}

void Channel::setListener(Listener* listener)
{
    listener_ = listener;
}

void Channel::connectToServer(const QString& channel_name)
{
    socket_->connectToServer(channel_name);
}

void Channel::start()
{
    if (!is_connected_)
        return;

    // Start receiving messages.
    onReadyRead();
}

void Channel::send(const QByteArray& buffer)
{
    const bool schedule_write = write_queue_.empty();

    write_queue_.emplace(buffer);

    if (schedule_write)
        scheduleWrite();
}

void Channel::onError(QLocalSocket::LocalSocketError /* socket_error */)
{
    LOG(LS_WARNING) << "IPC channel error: " << socket_->errorString();
}

void Channel::onBytesWritten(int64_t bytes)
{
    const QByteArray& write_buffer = write_queue_.front();

    written_ += bytes;

    if (written_ < sizeof(uint32_t))
    {
        socket_->write(reinterpret_cast<const char*>(&write_size_) + written_,
                       sizeof(uint32_t) - written_);
    }
    else if (written_ < sizeof(uint32_t) + write_buffer.size())
    {
        socket_->write(write_buffer.data() + (written_ - sizeof(uint32_t)),
                       write_buffer.size() - (written_ - sizeof(uint32_t)));
    }
    else
    {
        write_queue_.pop();
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
                                    sizeof(uint32_t) - read_);
            if (current + read_ == sizeof(uint32_t))
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

            if (listener_)
                listener_->onIpcMessage(read_buffer_);
            continue;
        }

        if (current <= 0)
            break;

        read_ += current;
    }
}

void Channel::init()
{
    qRegisterMetaType<QLocalSocket::LocalSocketError>();

    connect(socket_, &QLocalSocket::connected, [this]()
    {
#if defined(OS_WIN)
        HANDLE pipe_handle = reinterpret_cast<HANDLE>(socket_->socketDescriptor());
        peer_process_id_ = serverProcessIdImpl(pipe_handle);
        peer_session_id_ = serverSessionIdImpl(pipe_handle);
#endif // defined(OS_WIN)

        if (listener_)
            listener_->onIpcConnected();
    });

    connect(socket_, &QLocalSocket::bytesWritten, this, &Channel::onBytesWritten);
    connect(socket_, &QLocalSocket::readyRead, this, &Channel::onReadyRead);

    connect(socket_, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error),
            this, &Channel::onError);

    connect(socket_, &QLocalSocket::disconnected, [this]()
    {
        if (listener_)
            listener_->onIpcDisconnected();
    });
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

    socket_->write(reinterpret_cast<const char*>(&write_size_), sizeof(uint32_t));
}

} // namespace ipc
