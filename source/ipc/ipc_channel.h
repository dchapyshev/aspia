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

#ifndef IPC__IPC_CHANNEL_H
#define IPC__IPC_CHANNEL_H

#include "base/process_handle.h"
#include "base/session_id.h"
#include "base/memory/byte_array.h"
#include "base/memory/scalable_queue.h"
#include "base/threading/thread_checker.h"

#include <asio/windows/stream_handle.hpp>

#include <filesystem>

namespace base {
class Location;
} // namespace base

namespace ipc {

class ChannelProxy;
class Listener;
class Server;

class Channel
{
public:
    Channel();
    ~Channel();

    std::shared_ptr<ChannelProxy> channelProxy();

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

    void send(base::ByteArray&& buffer);

    base::ProcessId peerProcessId() const { return peer_process_id_; }
    base::SessionId peerSessionId() const { return peer_session_id_; }
    std::filesystem::path peerFilePath() const;

private:
    friend class Server;
    friend class ChannelProxy;

    Channel(std::u16string_view channel_name, asio::windows::stream_handle&& stream);
    static std::u16string channelName(std::u16string_view channel_id);

    void onErrorOccurred(const base::Location& location, const std::error_code& error_code);
    void doWrite();
    void doReadMessage();
    void onMessageReceived();

    std::u16string channel_name_;
    asio::windows::stream_handle stream_;

    std::shared_ptr<ChannelProxy> proxy_;
    Listener* listener_ = nullptr;

    bool is_connected_ = false;
    bool is_paused_ = true;

    base::ScalableQueue<base::ByteArray> write_queue_;
    uint32_t write_size_ = 0;

    uint32_t read_size_ = 0;
    base::ByteArray read_buffer_;

    base::ProcessId peer_process_id_ = base::kNullProcessId;
    base::SessionId peer_session_id_ = base::kInvalidSessionId;

    THREAD_CHECKER(thread_checker_);

    DISALLOW_COPY_AND_ASSIGN(Channel);
};

} // namespace ipc

#endif // IPC__IPC_CHANNEL_H
