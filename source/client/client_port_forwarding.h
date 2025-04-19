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

#ifndef CLIENT_CLIENT_PORT_FORWARDING_H
#define CLIENT_CLIENT_PORT_FORWARDING_H

#include "base/waitable_timer.h"
#include "base/memory/local_memory.h"
#include "client/client.h"
#include "proto/port_forwarding.pb.h"

#include <queue>

namespace client {

class PortForwardingWindowProxy;

class ClientPortForwarding final
    : public Client
{
public:
    explicit ClientPortForwarding(std::shared_ptr<base::TaskRunner> io_task_runner);
    ~ClientPortForwarding() final;

    void setPortForwardingWindow(
        std::shared_ptr<PortForwardingWindowProxy> port_forwarding_window_proxy);
    void setPortForwardingConfig(const proto::port_forwarding::Config& config);

protected:
    // Client implementation.
    void onSessionStarted() final;
    void onSessionMessageReceived(uint8_t channel_id, const base::ByteArray& buffer) final;
    void onSessionMessageWritten(uint8_t channel_id, size_t pending) final;

private:
    void onAccept(const std::error_code& error_code, asio::ip::tcp::socket socket);
    void onWrite(const std::error_code& error_code, size_t bytes_transferred);
    void onRead(const std::error_code& error_code, size_t bytes_transferred);

    void doWrite();
    void doRead();

    void sendPortForwardingRequest();
    void sendPortForwardingStart();
    void sendPortForwardingData(const char* buffer, size_t length);

    static void startCommandLine(std::string_view command_line);

    enum class State
    {
        DISCONNECED = 0,
        ACCEPTING = 1,
        CONNECTED = 2
    };

    std::shared_ptr<PortForwardingWindowProxy> port_forwarding_window_proxy_;

    State state_ = State::DISCONNECED;
    bool is_started_ = false;

    std::string remote_host_;
    uint16_t remote_port_ = 0;
    uint16_t local_port_ = 0;
    std::string command_line_;

    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    std::unique_ptr<asio::ip::tcp::socket> socket_;

    std::unique_ptr<proto::port_forwarding::HostToClient> incoming_message_;
    std::unique_ptr<proto::port_forwarding::ClientToHost> outgoing_message_;

    class Handler;
    base::local_shared_ptr<Handler> handler_;

    std::queue<std::string> write_queue_;

    static const int kBufferSize = 8192;
    std::array<char, kBufferSize> read_buffer_;

    base::WaitableTimer statistics_timer_;
    uint64_t rx_bytes_ = 0;
    uint64_t tx_bytes_ = 0;
};

} // namespace client

#endif // CLIENT_CLIENT_PORT_FORWARDING_H
