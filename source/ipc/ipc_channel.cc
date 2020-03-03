//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/location.h"
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

#include <psapi.h>

namespace ipc {

namespace {

const char16_t kPipeNamePrefix[] = u"\\\\.\\pipe\\aspia.";
const uint32_t kMaxMessageSize = 16 * 1024 * 1024; // 16MB
const DWORD kConnectTimeout = 5000; // ms

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

base::SessionId clientSessionIdImpl(HANDLE pipe_handle)
{
    ULONG session_id = base::kInvalidSessionId;

    if (!GetNamedPipeClientSessionId(pipe_handle, &session_id))
    {
        PLOG(LS_WARNING) << "GetNamedPipeClientSessionId failed";
        return base::kInvalidSessionId;
    }

    return session_id;
}

base::SessionId serverSessionIdImpl(HANDLE pipe_handle)
{
    ULONG session_id = base::kInvalidSessionId;

    if (!GetNamedPipeServerSessionId(pipe_handle, &session_id))
    {
        PLOG(LS_WARNING) << "GetNamedPipeServerSessionId failed";
        return base::kInvalidSessionId;
    }

    return session_id;
}

} // namespace

Channel::Channel()
    : stream_(base::MessageLoop::current()->pumpAsio()->ioContext()),
      proxy_(new ChannelProxy(base::MessageLoop::current()->taskRunner(), this))
{
    // Nothing
}

Channel::Channel(std::u16string_view channel_name, asio::windows::stream_handle&& stream)
    : channel_name_(channel_name),
      stream_(std::move(stream)),
      proxy_(new ChannelProxy(base::MessageLoop::current()->taskRunner(), this)),
      is_connected_(true)
{
    peer_process_id_ = clientProcessIdImpl(stream_.native_handle());
    peer_session_id_ = clientSessionIdImpl(stream_.native_handle());
}

Channel::~Channel()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    proxy_->willDestroyCurrentChannel();
    proxy_ = nullptr;

    listener_ = nullptr;

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
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    const DWORD flags = SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION | FILE_FLAG_OVERLAPPED;
    channel_name_ = channelName(channel_id);

    base::win::ScopedHandle handle;

    while (true)
    {
        handle.reset(CreateFileW(reinterpret_cast<const wchar_t*>(channel_name_.c_str()),
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
                            << base::SystemError::toString(error_code);
            return false;
        }

        if (!WaitNamedPipeW(reinterpret_cast<const wchar_t*>(channel_name_.c_str()),
                            kConnectTimeout))
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
    return true;
}

void Channel::disconnect()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (!is_connected_)
        return;

    is_connected_ = false;

    std::error_code ignored_code;

    stream_.cancel(ignored_code);
    stream_.close(ignored_code);
}

bool Channel::isConnected() const
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return is_connected_;
}

bool Channel::isPaused() const
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return is_paused_;
}

void Channel::pause()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    is_paused_ = true;
}

void Channel::resume()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

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
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    const bool schedule_write = write_queue_.empty();

    // Add the buffer to the queue for sending.
    write_queue_.emplace(std::move(buffer));

    if (schedule_write)
        doWrite();
}

std::filesystem::path Channel::peerFilePath() const
{
    base::win::ScopedHandle process(
        OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, peer_process_id_));
    if (!process.isValid())
    {
        PLOG(LS_WARNING) << "OpenProcess failed";
        return std::filesystem::path();
    }

    wchar_t buffer[MAX_PATH] = { 0 };

    if (!GetModuleFileNameExW(process.get(), nullptr, buffer, std::size(buffer)))
    {
        PLOG(LS_WARNING) << "GetModuleFileNameExW failed";
        return std::filesystem::path();
    }

    return buffer;
}

// static
std::u16string Channel::channelName(std::u16string_view channel_id)
{
    std::u16string name(kPipeNamePrefix);
    name.append(channel_id);
    return name;
}

void Channel::onErrorOccurred(const base::Location& location, const std::error_code& error_code)
{
    if (error_code == asio::error::operation_aborted)
        return;

    LOG(LS_WARNING) << "Error in IPC channel '" << channel_name_ << "': "
                    << base::utf16FromLocal8Bit(error_code.message())
                    << " (code: " << error_code.value()
                    << ", location: " << location.toString() << ")";

    disconnect();

    if (listener_)
    {
        listener_->onDisconnected();
        listener_ = nullptr;
    }
}

void Channel::doWrite()
{
    write_size_ = write_queue_.front().size();

    if (!write_size_ || write_size_ > kMaxMessageSize)
    {
        onErrorOccurred(FROM_HERE, asio::error::message_size);
        return;
    }

    asio::async_write(stream_, asio::buffer(&write_size_, sizeof(write_size_)),
        [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        DCHECK_EQ(bytes_transferred, sizeof(write_size_));
        DCHECK(!write_queue_.empty());

        const base::ByteArray& buffer = write_queue_.front();

        // Send the buffer to the recipient.
        asio::async_write(stream_, asio::buffer(buffer.data(), buffer.size()),
            [this](const std::error_code& error_code, size_t bytes_transferred)
        {
            if (error_code)
            {
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            DCHECK_EQ(bytes_transferred, write_size_);
            DCHECK(!write_queue_.empty());

            // Delete the sent message from the queue.
            write_queue_.pop();

            // If the queue is not empty, then we send the following message.
            if (write_queue_.empty() && !proxy_->reloadWriteQueue(&write_queue_))
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
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        DCHECK_EQ(bytes_transferred, sizeof(read_size_));

        if (!read_size_ || read_size_ > kMaxMessageSize)
        {
            onErrorOccurred(FROM_HERE, asio::error::message_size);
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
