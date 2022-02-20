//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_IPC_CHANNEL_H
#define BASE_IPC_CHANNEL_H

#include "base/process_handle.h"
#include "base/session_id.h"
#include "base/memory/byte_array.h"
#include "base/threading/thread_checker.h"

#if defined(OS_WIN)
#include <asio/windows/stream_handle.hpp>
#elif defined(OS_POSIX)
#include <asio/posix/stream_descriptor.hpp>
#endif

#include <filesystem>
#include <queue>

namespace base {

class IpcChannelProxy;
class IpcServer;
class Location;

class IpcChannel
{
public:
    IpcChannel();
    ~IpcChannel();

    class Listener
    {
    public:
        virtual ~Listener() = default;

        virtual void onDisconnected() = 0;
        virtual void onMessageReceived(const ByteArray& buffer) = 0;
    };

    std::shared_ptr<IpcChannelProxy> channelProxy();

    // Sets an instance of the class to receive connection status notifications or new messages.
    // You can change this in the process.
    void setListener(Listener* listener);

    [[nodiscard]]
    bool connect(std::u16string_view channel_id);

    void disconnect();

    bool isConnected() const;
    bool isPaused() const;

    void pause();
    void resume();

    void send(ByteArray&& buffer);

    ProcessId peerProcessId() const { return peer_process_id_; }
    SessionId peerSessionId() const { return peer_session_id_; }
    std::filesystem::path peerFilePath() const;

private:
    friend class IpcServer;
    friend class IpcChannelProxy;

#if defined(OS_WIN)
    using Stream = asio::windows::stream_handle;
#elif defined(OS_POSIX)
    using Stream = asio::posix::stream_descriptor;
#endif

    IpcChannel(std::u16string_view channel_name, Stream&& stream);
    static std::u16string channelName(std::u16string_view channel_id);

    void onErrorOccurred(const Location& location, const std::error_code& error_code);
    void doWrite();
    void doReadMessage();
    void onMessageReceived();

    std::u16string channel_name_;
    Stream stream_;

    std::shared_ptr<IpcChannelProxy> proxy_;
    Listener* listener_ = nullptr;

    bool is_connected_ = false;
    bool is_paused_ = true;

    std::queue<ByteArray> write_queue_;
    uint32_t write_size_ = 0;

    uint32_t read_size_ = 0;
    ByteArray read_buffer_;

    ProcessId peer_process_id_ = kNullProcessId;
    SessionId peer_session_id_ = kInvalidSessionId;

    THREAD_CHECKER(thread_checker_)

    DISALLOW_COPY_AND_ASSIGN(IpcChannel);
};

} // namespace base

#endif // BASE_IPC_CHANNEL_H
