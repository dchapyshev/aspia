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

#include "base/ipc/ipc_channel.h"

#include <QTimer>

#include "base/location.h"
#include "base/threading/asio_event_dispatcher.h"

#include <asio/read.hpp>
#include <asio/write.hpp>

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include <sys/socket.h>
#endif // defined(Q_OS_LINUX)

#if defined(Q_OS_MACOS)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif // defined(Q_OS_MACOS)

namespace {

const int kWriteQueueReservedSize = 64;
const quint32 kMaxMessageSize = 16 * 1024 * 1024; // 16MB

// 'A','S','I','C' (Aspia Ipc Channel) read little-endian.
const quint32 kHeaderMagic = 0x43495341;

#if defined(Q_OS_UNIX)
const char kNamePrefix[] = "/tmp/aspia_";
using PipeHandle = int;
#endif // defined(Q_OS_UNIX)

#if defined(Q_OS_WINDOWS)
const char kNamePrefix[] = "\\\\.\\pipe\\aspia.";
const DWORD kConnectTimeout = 3000; // ms
using PipeHandle = HANDLE;
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
quint32 makeInstanceId()
{
    static thread_local quint32 instance_id = 0;
    ++instance_id;
    return instance_id;
}

//--------------------------------------------------------------------------------------------------
SessionId clientSessionId(PipeHandle pipe_handle)
{
#if defined(Q_OS_WINDOWS)
    ULONG session_id = kInvalidSessionId;
    if (!GetNamedPipeClientSessionId(pipe_handle, &session_id))
    {
        PLOG(ERROR) << "GetNamedPipeClientSessionId failed";
        return kInvalidSessionId;
    }

    return session_id;
#else
    Q_UNUSED(pipe_handle)
    return kInvalidSessionId;
#endif
}

//--------------------------------------------------------------------------------------------------
SessionId serverSessionId(PipeHandle pipe_handle)
{
#if defined(Q_OS_WINDOWS)
    ULONG session_id = kInvalidSessionId;
    if (!GetNamedPipeServerSessionId(pipe_handle, &session_id))
    {
        PLOG(ERROR) << "GetNamedPipeServerSessionId failed";
        return kInvalidSessionId;
    }

    return session_id;
#else
    return kInvalidSessionId;
#endif
}

//--------------------------------------------------------------------------------------------------
// Returns the PID of the peer process. |is_server_side| selects which Windows API to use
// (GetNamedPipeClientProcessId vs GetNamedPipeServerProcessId); on UNIX both sides of a domain
// socket are symmetric and the parameter is ignored.
quint32 peerProcessId(PipeHandle pipe_handle, bool is_server_side)
{
#if defined(Q_OS_WINDOWS)
    ULONG process_id = 0;
    const BOOL ok = is_server_side ?
        GetNamedPipeClientProcessId(pipe_handle, &process_id) :
        GetNamedPipeServerProcessId(pipe_handle, &process_id);
    if (!ok)
    {
        PLOG(ERROR) << "GetNamedPipe" << (is_server_side ? "Client" : "Server") << "ProcessId failed";
        return 0;
    }

    return process_id;
#elif defined(Q_OS_LINUX)
    Q_UNUSED(is_server_side)
    struct ucred cred;
    socklen_t len = sizeof(cred);
    if (getsockopt(pipe_handle, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0)
    {
        PLOG(ERROR) << "getsockopt(SO_PEERCRED) failed";
        return 0;
    }

    return static_cast<quint32>(cred.pid);
#elif defined(Q_OS_MACOS)
    Q_UNUSED(is_server_side)
    pid_t pid = 0;
    socklen_t len = sizeof(pid);
    if (getsockopt(pipe_handle, SOL_LOCAL, LOCAL_PEERPID, &pid, &len) != 0)
    {
        PLOG(ERROR) << "getsockopt(LOCAL_PEERPID) failed";
        return 0;
    }

    return static_cast<quint32>(pid);
#else
    Q_UNUSED(pipe_handle)
    Q_UNUSED(is_server_side)
    return 0;
#endif
}

} // namespace

//--------------------------------------------------------------------------------------------------
IpcChannel::IpcChannel(QObject* parent)
    : QObject(parent),
      instance_id_(makeInstanceId()),
      stream_(AsioEventDispatcher::ioContext())
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
IpcChannel::IpcChannel(const QString& channel_name, Stream&& stream, QObject* parent)
    : QObject(parent),
      instance_id_(makeInstanceId()),
      session_id_(clientSessionId(stream.native_handle())),
      process_id_(peerProcessId(stream.native_handle(), /* is_server_side */ true)),
      channel_name_(channel_name),
      stream_(std::move(stream)),
      is_connected_(true)
{
    io_->write_queue.reserve(kWriteQueueReservedSize);
}

//--------------------------------------------------------------------------------------------------
IpcChannel::~IpcChannel()
{
    io_->alive = false;
    disconnectFrom();
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::connectTo(const QString& channel_name)
{
    channel_name_ = channel_name;
    scheduleConnectAttempt(Milliseconds(0), 0);
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
void IpcChannel::setPaused(bool enable)
{
    if (!is_connected_ || is_paused_ == enable)
        return;

    is_paused_ = enable;
    if (is_paused_)
        return;

    switch (read_state_)
    {
        // We already have an incomplete read operation.
        case ReadState::READ_HEADER:
        case ReadState::READ_DATA:
            return;

        // If we have a message that was received before the pause command.
        case ReadState::PENDING:
            onMessageReceived();
            break;

        default:
            break;
    }

    CDCHECK_EQ(io_->read_header.message_size, 0);
    doReadHeader();
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::send(quint32 channel_id, const QByteArray& buffer, bool reliable)
{
    const bool schedule_write = io_->write_queue.empty();

    // Add the buffer to the queue for sending.
    io_->write_queue.emplace_back(channel_id, buffer, reliable);

    if (schedule_write)
        doWriteHeader();
}

//--------------------------------------------------------------------------------------------------
// static
QString IpcChannel::channelPath(const QString& channel_name)
{
    QString name(kNamePrefix);
    name.append(channel_name);
    return name;
}

//--------------------------------------------------------------------------------------------------
bool IpcChannel::connectAttempt()
{
    if (channel_name_.isEmpty())
    {
        CLOG(ERROR) << "Empty channel name";
        return false;
    }

    const QString channel_path = channelPath(channel_name_);

#if defined(Q_OS_WINDOWS)
    const DWORD flags = SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION | FILE_FLAG_OVERLAPPED;
    ScopedHandle handle;

    while (true)
    {
        handle.reset(CreateFileW(qUtf16Printable(channel_path), GENERIC_WRITE | GENERIC_READ, 0,
            nullptr, OPEN_EXISTING, flags, nullptr));
        if (handle.isValid())
            break;

        DWORD error_code = GetLastError();

        if (error_code != ERROR_PIPE_BUSY)
        {
            CLOG(ERROR) << "Failed to connect to the named pipe:" << SystemError::toString(error_code)
                        << "(channel" << channel_name_ << ")";
            return false;
        }

        if (!WaitNamedPipeW(qUtf16Printable(channel_path), kConnectTimeout))
        {
            PLOG(ERROR) << "WaitNamedPipeW failed (channel" << channel_name_ << ")";
            return false;
        }
    }

    std::error_code error_code;
    stream_.assign(handle.get(), error_code);
    if (error_code)
    {
        // On failure the stream does not take ownership; ScopedHandle closes the handle.
        CLOG(ERROR) << "Failed to assign handle:" << error_code;
        return false;
    }

    // The stream owns the handle now.
    handle.release();
#else
    asio::local::stream_protocol::endpoint endpoint(channel_path.toLocal8Bit().toStdString());
    std::error_code error_code;
    stream_.connect(endpoint, error_code);
    if (error_code)
    {
        CLOG(ERROR) << "Unable to connect:" << error_code;
        return false;
    }
#endif

    session_id_ = serverSessionId(stream_.native_handle());
    process_id_ = peerProcessId(stream_.native_handle(), /* is_server_side */ false);
    return true;
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::scheduleConnectAttempt(Milliseconds delay, int count)
{
    if (count > 30)
    {
        CLOG(ERROR) << "Connection timeout (channel" << channel_name_ << ")";
        emit sig_errorOccurred();
        return;
    }

    QTimer::singleShot(delay, this, [this, count]()
    {
        if (connectAttempt())
        {
            is_connected_ = true;
            emit sig_connected();
            return;
        }

        scheduleConnectAttempt(Milliseconds(100), count + 1);
    });
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::disconnectFrom()
{
    if (!is_connected_)
        return;

    is_connected_ = false;

    std::error_code ignored_code;
    stream_.cancel(ignored_code);
    stream_.close(ignored_code);
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::onErrorOccurred(const Location& location, const std::error_code& error_code)
{
    CLOG(ERROR) << "Error in IPC channel" << channel_name_ << ":" << error_code << "(from" << location << ")";
    disconnectFrom();
    emit sig_disconnected();
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::onMessageReceived()
{
    emit sig_messageReceived(io_->read_header.channel_id, io_->read_buffer, !!io_->read_header.reliable);
    memset(&io_->read_header, 0, sizeof(Header));
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::doWriteHeader()
{
    const WriteTask& task = io_->write_queue.front();

    io_->write_header.magic = kHeaderMagic;
    io_->write_header.message_size = task.data().size();
    io_->write_header.channel_id = task.channelId();
    io_->write_header.reliable = task.reliable() ? 1 : 0;

    if (!io_->write_header.message_size || io_->write_header.message_size > kMaxMessageSize)
    {
        onErrorOccurred(FROM_HERE, asio::error::message_size);
        return;
    }

    auto io = io_;
    asio::async_write(stream_,
                      asio::buffer(&io_->write_header, sizeof(io_->write_header)),
                      [this, io](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!io->alive)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;

            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        CDCHECK_EQ(bytes_transferred, sizeof(io_->write_header));
        doWriteData();
    });
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::doWriteData()
{
    CDCHECK(!io_->write_queue.empty());

    const QByteArray& buffer = io_->write_queue.front().data();

    // Send the buffer to the recipient.
    auto io = io_;
    asio::async_write(stream_, asio::buffer(buffer.data(), buffer.size()),
                      [this, io](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!io->alive)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;

            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        CDCHECK_EQ(bytes_transferred, io_->write_header.message_size);
        CDCHECK(!io_->write_queue.empty());

        // Delete the sent message from the queue.
        io_->write_queue.pop_front();

        // If the queue is not empty, then we send the following message.
        if (io_->write_queue.empty())
            return;

        doWriteHeader();
    });
}

//--------------------------------------------------------------------------------------------------
void IpcChannel::doReadHeader()
{
    read_state_ = ReadState::READ_HEADER;

    auto io = io_;
    asio::async_read(stream_,
                     asio::buffer(&io_->read_header, sizeof(io_->read_header)),
                     [this, io](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!io->alive)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;

            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        CDCHECK_EQ(bytes_transferred, sizeof(io_->read_header));

        if (io_->read_header.magic != kHeaderMagic)
        {
            CLOG(ERROR) << "Invalid header magic:" << Qt::hex << io_->read_header.magic;
            onErrorOccurred(FROM_HERE, asio::error::invalid_argument);
            return;
        }

        if (!io_->read_header.message_size || io_->read_header.message_size > kMaxMessageSize)
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
    read_state_ = ReadState::READ_DATA;

    if (io_->read_buffer.capacity() < static_cast<qsizetype>(io_->read_header.message_size))
    {
        io_->read_buffer.clear();
        io_->read_buffer.reserve(io_->read_header.message_size);
    }

    io_->read_buffer.resize(io_->read_header.message_size);

    auto io = io_;
    asio::async_read(stream_,
                     asio::buffer(io_->read_buffer.data(), io_->read_buffer.size()),
                     [this, io](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!io->alive)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;

            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        CDCHECK_EQ(bytes_transferred, io_->read_header.message_size);

        if (is_paused_)
        {
            read_state_ = ReadState::PENDING;
            return;
        }

        onMessageReceived();

        if (is_paused_)
        {
            read_state_ = ReadState::IDLE;
            return;
        }

        CDCHECK_EQ(io_->read_header.message_size, 0);
        doReadHeader();
    });
}
