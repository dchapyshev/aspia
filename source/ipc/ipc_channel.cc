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

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_asio.h"
#include "base/strings/unicode.h"
#include "base/win/scoped_object.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"

#include <asio/read.hpp>
#include <asio/write.hpp>
#include <asio/post.hpp>

#include <functional>

namespace ipc {

namespace {

const char16_t kPipeNamePrefix[] = u"\\\\.\\pipe\\aspia.";
const uint32_t kMaxMessageSize = 16 * 1024 * 1024; // 16MB

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

} // namespace

Channel::Channel()
    : io_context_(base::MessageLoop::current()->pumpAsio()->ioContext()),
      stream_(io_context_),
      proxy_(new ChannelProxy(this))
{
    // Nothing
}

Channel::Channel(asio::io_context& io_context, asio::windows::stream_handle&& stream)
    : io_context_(io_context),
      stream_(std::move(stream)),
      proxy_(new ChannelProxy(this)),
      is_connected_(true)
{
    peer_process_id_ = clientProcessIdImpl(stream_.native_handle());
    peer_session_id_ = clientSessionIdImpl(stream_.native_handle());
}

Channel::~Channel()
{
    proxy_->willDestroyCurrentChannel();
    proxy_ = nullptr;

    disconnect();
}

std::shared_ptr<ChannelProxy> Channel::channelProxy()
{
    return proxy_;
}

void Channel::setListener(Listener* listener)
{
    listener_ = listener;
}

bool Channel::connect(std::u16string_view channel_id)
{
    const DWORD flags = SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION | FILE_FLAG_OVERLAPPED;
    std::u16string channel_name = channelName(channel_id);

    base::win::ScopedHandle handle;

    while (true)
    {
        handle.reset(CreateFileW(reinterpret_cast<const wchar_t*>(channel_name.c_str()),
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
            LOG(LS_WARNING) << "Failed to connect to the named pipe: "
                            << base::systemErrorCodeToString(error_code);
            return false;
        }

        if (!WaitNamedPipeW(reinterpret_cast<const wchar_t*>(channel_name.c_str()), 5000))
        {
            PLOG(LS_WARNING) << "WaitNamedPipeW failed";
            return false;
        }
    }

    std::error_code error_code;
    stream_.assign(handle.release(), error_code);
    if (error_code)
        return false;

    peer_process_id_ = serverProcessIdImpl(stream_.native_handle());
    peer_session_id_ = serverSessionIdImpl(stream_.native_handle());

    is_connected_ = true;

    asio::post(io_context_, [this]()
    {
        if (listener_)
            listener_->onConnected();
    });

    return true;
}

void Channel::disconnect()
{
    if (!is_connected_)
        return;

    is_connected_ = false;

    std::error_code ignored_code;
    stream_.close(ignored_code);

    reloadWriteQueue();

    // Remove all messages from the write queue.
    while (!work_write_queue_.empty())
        work_write_queue_.pop();

    if (listener_)
    {
        listener_->onDisconnected();
        listener_ = nullptr;
    }
}

bool Channel::isConnected() const
{
    return is_connected_;
}

bool Channel::isPaused() const
{
    return is_paused_;
}

void Channel::pause()
{
    is_paused_ = true;
}

void Channel::resume()
{
    if (!is_connected_ || !is_paused_)
        return;

    is_paused_ = false;

    // If we have a message that was received before the pause command.
    if (read_size_)
        onMessageReceived();

    DCHECK_EQ(read_size_, 0);

    doReadMessage();
}

void Channel::send(base::ByteArray&& buffer)
{
    std::scoped_lock lock(incoming_write_queue_lock_);

    const bool schedule_write = incoming_write_queue_.empty();

    // Add the buffer to the queue for sending.
    incoming_write_queue_.emplace(std::move(buffer));

    if (!schedule_write)
        return;

    asio::post(io_context_, std::bind(&Channel::scheduleWrite, this));
}

// static
std::u16string Channel::channelName(std::u16string_view channel_id)
{
    std::u16string name(kPipeNamePrefix);
    name.append(channel_id);
    return name;
}

void Channel::onErrorOccurred(const std::error_code& error_code)
{
    if (error_code == asio::error::operation_aborted)
        return;

    LOG(LS_WARNING) << "Error in the IPC channel: "
                    << base::utf16FromLocal8Bit(error_code.message());

    disconnect();
}

bool Channel::reloadWriteQueue()
{
    if (!work_write_queue_.empty())
        return false;

    std::scoped_lock lock(incoming_write_queue_lock_);

    if (incoming_write_queue_.empty())
        return false;

    incoming_write_queue_.fastSwap(work_write_queue_);
    DCHECK(incoming_write_queue_.empty());

    return true;
}

void Channel::scheduleWrite()
{
    if (!reloadWriteQueue())
        return;

    doWrite();
}

void Channel::doWrite()
{
    write_size_ = work_write_queue_.front().size();

    if (!write_size_ || write_size_ > kMaxMessageSize)
    {
        onErrorOccurred(asio::error::message_size);
        return;
    }

    asio::async_write(stream_, asio::buffer(&write_size_, sizeof(write_size_)),
        [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            onErrorOccurred(error_code);
            return;
        }

        DCHECK_EQ(bytes_transferred, sizeof(write_size_));
        DCHECK(!work_write_queue_.empty());

        const base::ByteArray& buffer = work_write_queue_.front();

        // Send the buffer to the recipient.
        asio::async_write(stream_, asio::buffer(buffer.data(), buffer.size()),
            [this](const std::error_code& error_code, size_t bytes_transferred)
        {
            if (error_code)
            {
                onErrorOccurred(error_code);
                return;
            }

            DCHECK_EQ(bytes_transferred, write_size_);
            DCHECK(!work_write_queue_.empty());

            // Delete the sent message from the queue.
            work_write_queue_.pop();

            // If the queue is not empty, then we send the following message.
            if (work_write_queue_.empty() && !reloadWriteQueue())
                return;

            doWrite();
        });
    });
}

void Channel::doReadMessage()
{
    asio::async_read(stream_, asio::buffer(&read_size_, sizeof(read_size_)),
        [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            onErrorOccurred(error_code);
            return;
        }

        DCHECK_EQ(bytes_transferred, sizeof(read_size_));

        if (!read_size_ || read_size_ > kMaxMessageSize)
        {
            onErrorOccurred(asio::error::message_size);
            return;
        }

        if (read_buffer_.capacity() < read_size_)
            read_buffer_.reserve(read_size_);

        read_buffer_.resize(read_size_);

        asio::async_read(stream_, asio::buffer(read_buffer_.data(), read_buffer_.size()),
            [this](const std::error_code& error_code, size_t bytes_transferred)
        {
            if (error_code)
            {
                onErrorOccurred(error_code);
                return;
            }

            DCHECK_EQ(bytes_transferred, read_size_);

            if (is_paused_)
                return;

            onMessageReceived();

            if (is_paused_)
                return;

            DCHECK_EQ(read_size_, 0);

            doReadMessage();
        });
    });
}

void Channel::onMessageReceived()
{
    if (listener_)
        listener_->onMessageReceived(read_buffer_);

    read_size_ = 0;
}

} // namespace ipc
