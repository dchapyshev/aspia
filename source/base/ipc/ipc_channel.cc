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

#include "base/ipc/ipc_channel.h"

#include "base/asio_event_dispatcher.h"
#include "base/location.h"
#include "base/logging.h"

#include <asio/read.hpp>
#include <asio/write.hpp>

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
#endif // defined(Q_OS_WINDOWS)

namespace base {

namespace {

const int kWriteQueueReservedSize = 64;
const quint32 kMaxMessageSize = 16 * 1024 * 1024; // 16MB

#if defined(Q_OS_UNIX)
const char kLocalSocketPrefix[] = "/tmp/aspia_";
#endif // defined(Q_OS_UNIX)

#if defined(Q_OS_WINDOWS)

const char kPipeNamePrefix[] = "\\\\.\\pipe\\aspia.";
const DWORD kConnectTimeout = 5000; // ms

//--------------------------------------------------------------------------------------------------
SessionId clientSessionIdImpl(HANDLE pipe_handle)
{
    ULONG session_id = kInvalidSessionId;

    if (!GetNamedPipeClientSessionId(pipe_handle, &session_id))
    {
        PLOG(ERROR) << "GetNamedPipeClientSessionId failed";
        return kInvalidSessionId;
    }

    return session_id;
}

//--------------------------------------------------------------------------------------------------
SessionId serverSessionIdImpl(HANDLE pipe_handle)
{
    ULONG session_id = kInvalidSessionId;

    if (!GetNamedPipeServerSessionId(pipe_handle, &session_id))
    {
        PLOG(ERROR) << "GetNamedPipeServerSessionId failed";
        return kInvalidSessionId;
    }

    return session_id;
}

#endif // defined(Q_OS_WINDOWS)

} // namespace

//--------------------------------------------------------------------------------------------------
IpcChannel::IpcChannel(QObject* parent)
    : QObject(parent),
      stream_(AsioEventDispatcher::currentIoContext())
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
IpcChannel::IpcChannel(const QString& channel_name, Stream&& stream, QObject* parent)
    : QObject(parent),
      channel_name_(channel_name),
      stream_(std::move(stream)),
      is_connected_(true)
{
    LOG(INFO) << "Ctor";

    write_queue_.reserve(kWriteQueueReservedSize);

#if defined(Q_OS_WINDOWS)
    peer_session_id_ = clientSessionIdImpl(stream_.native_handle());
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
IpcChannel::~IpcChannel()
{
    LOG(INFO) << "Dtor";
    disconnectFrom();
}

//--------------------------------------------------------------------------------------------------
bool IpcChannel::connectTo(const QString& channel_id)
{
    if (channel_id.isEmpty())
    {
        LOG(ERROR) << "Empty channel id";
        return false;
    }

    channel_name_ = channelName(channel_id);

#if defined(Q_OS_WINDOWS)
    const DWORD flags = SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION | FILE_FLAG_OVERLAPPED;

    ScopedHandle handle;

    while (true)
    {
        handle.reset(CreateFileW(qUtf16Printable(channel_name_),
                                 GENERIC_WRITE | GENERIC_READ,
                                 0,
                                 nullptr,
                                 OPEN_EXISTING,
                                 flags,
                                 nullptr));
        if (handle.isValid())
            break;

        DWORD error_code = GetLastError();

        if (error_code != ERROR_PIPE_BUSY)
        {
            LOG(ERROR) << "Failed to connect to the named pipe:"
                       << SystemError::toString(error_code) << "(channel_name="
                       << channel_name_ << ")";
            return false;
        }

        if (!WaitNamedPipeW(qUtf16Printable(channel_name_), kConnectTimeout))
        {
            PLOG(ERROR) << "WaitNamedPipeW failed (channel_name=" << channel_name_ << ")";
            return false;
        }
    }

    std::error_code error_code;
    stream_.assign(handle.release(), error_code);
    if (error_code)
    {
        LOG(ERROR) << "Failed to assign handle:" << error_code;
        return false;
    }

    peer_session_id_ = serverSessionIdImpl(stream_.native_handle());

    is_connected_ = true;
    return true;
#else
    asio::local::stream_protocol::endpoint endpoint(base::local8BitFromUtf16(channel_name_));
    std::error_code error_code;
    stream_.connect(endpoint, error_code);
    if (error_code)
    {
        LOG(ERROR) << "Unable to connect:" << error_code;
        return false;
    }

    is_connected_ = true;
    return true;
#endif
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::disconnectFrom()
{
    if (!is_connected_)
    {
        LOG(INFO) << "Channel not in connected state";
        return;
    }

    LOG(INFO) << "disconnect channel (channel_name=" << channel_name_ << ")";
    is_connected_ = false;

    std::error_code ignored_code;

    stream_.cancel(ignored_code);
    stream_.close(ignored_code);
}

//--------------------------------------------------------------------------------------------------
bool IpcChannel::isConnected() const
{
    return is_connected_;
}

//--------------------------------------------------------------------------------------------------
bool IpcChannel::isPaused() const
{
    return is_paused_;
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::pause()
{
    is_paused_ = true;
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::resume()
{
    if (!is_connected_ || !is_paused_)
        return;

    LOG(INFO) << "resume channel (channel_name=" << channel_name_ << ")";
    is_paused_ = false;

    // If we have a message that was received before the pause command.
    if (read_size_)
        onMessageReceived();

    DCHECK_EQ(read_size_, 0);
    doReadSize();
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::send(const QByteArray& buffer)
{
    const bool schedule_write = write_queue_.empty();

    // Add the buffer to the queue for sending.
    write_queue_.push_back(buffer);

    if (schedule_write)
        doWriteSize();
}

//--------------------------------------------------------------------------------------------------
// static
QString IpcChannel::channelName(const QString& channel_id)
{
#if defined(Q_OS_WINDOWS)
    QString name(kPipeNamePrefix);
    name.append(channel_id);
    return name;
#else
    std::u16string name(kLocalSocketPrefix);
    name.append(channel_id);
    return name;
#endif
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::onErrorOccurred(const Location& location, const std::error_code& error_code)
{
    LOG(ERROR) << "Error in IPC channel" << channel_name_ << ":" << error_code
               << "(location=" << location.toString() << ")";

    disconnectFrom();
    emit sig_disconnected();
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::onMessageReceived()
{
    emit sig_messageReceived(read_buffer_);
    read_size_ = 0;
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::doWriteSize()
{
    write_size_ = static_cast<quint32>(write_queue_.front().size());

    if (!write_size_ || write_size_ > kMaxMessageSize)
    {
        onErrorOccurred(FROM_HERE, asio::error::message_size);
        return;
    }

    asio::async_write(stream_,
                      asio::buffer(&write_size_, sizeof(write_size_)),
                      [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;

            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        DCHECK_EQ(bytes_transferred, sizeof(write_size_));
        doWriteData();
    });
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::doWriteData()
{
    DCHECK(!write_queue_.empty());

    const QByteArray& buffer = write_queue_.front();

    // Send the buffer to the recipient.
    asio::async_write(stream_, asio::buffer(buffer.data(), buffer.size()),
                      [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;

            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        DCHECK_EQ(bytes_transferred, write_size_);
        DCHECK(!write_queue_.empty());

        // Delete the sent message from the queue.
        write_queue_.pop_front();

        // If the queue is not empty, then we send the following message.
        if (write_queue_.empty())
            return;

        doWriteSize();
    });
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::doReadSize()
{
    asio::async_read(stream_,
                     asio::buffer(&read_size_, sizeof(read_size_)),
                     [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;

            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        DCHECK_EQ(bytes_transferred, sizeof(read_size_));

        if (!read_size_ || read_size_ > kMaxMessageSize)
        {
            onErrorOccurred(FROM_HERE, asio::error::message_size);
            return;
        }

        doReadData();
    });
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::doReadData()
{
    if (read_buffer_.capacity() < static_cast<QByteArray::size_type>(read_size_))
    {
        read_buffer_.clear();
        read_buffer_.reserve(read_size_);
    }

    read_buffer_.resize(read_size_);

    asio::async_read(stream_,
                     asio::buffer(read_buffer_.data(), read_buffer_.size()),
                     [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;

            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        DCHECK_EQ(bytes_transferred, read_size_);

        if (is_paused_)
            return;

        onMessageReceived();

        if (is_paused_)
            return;

        DCHECK_EQ(read_size_, 0);
        doReadSize();
    });
}

} // namespace base
