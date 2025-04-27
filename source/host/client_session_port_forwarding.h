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

#ifndef HOST_CLIENT_SESSION_PORT_FORWARDING_H
#define HOST_CLIENT_SESSION_PORT_FORWARDING_H

#include "base/macros_magic.h"
#include "base/location.h"

#include "base/memory/local_memory.h"
#include "host/client_session.h"
#include "proto/port_forwarding.pb.h"

#include <array>
#include <queue>

#include <asio/ip/tcp.hpp>

namespace host {

class ClientSessionPortForwarding final : public ClientSession
{
    Q_OBJECT

public:
    ClientSessionPortForwarding(std::unique_ptr<base::TcpChannel> channel, QObject* parent);
    ~ClientSessionPortForwarding() final;

protected:
    // ClientSession implementation.
    void onStarted() final;
    void onReceived(uint8_t channel_id, const QByteArray& buffer) final;
    void onWritten(uint8_t channel_id, size_t pending) final;

private:
    void onError(const base::Location& location);
    void onResolved(const std::error_code& error_code,
                    const asio::ip::tcp::resolver::results_type& endpoints);
    void onConnected(const std::error_code& error_code, const asio::ip::tcp::endpoint& endpoint);
    void onWrite(const std::error_code& error_code, size_t bytes_transferred);
    void onRead(const std::error_code& error_code, size_t bytes_transferred);

    void doWrite();
    void doRead();

    void sendPortForwardingResult(proto::port_forwarding::Result::ErrorCode error_code);
    void sendPortForwardingData(const char* buffer, size_t length);

    enum class State
    {
        DISCONNECTED = 0,
        RESOLVING    = 1,
        CONNECTING   = 2,
        CONNECTED    = 3
    };

    State state_ = State::DISCONNECTED;
    bool forwarding_result_sended_ = false;
    bool is_started_ = false;

    asio::ip::tcp::socket socket_;
    asio::ip::tcp::resolver resolver_;

    class Handler;
    base::local_shared_ptr<Handler> handler_;

    proto::port_forwarding::ClientToHost incoming_message_;
    proto::port_forwarding::HostToClient outgoing_message_;

    std::queue<std::string> write_queue_;

    static const int kBufferSize = 8192;
    std::array<char, kBufferSize> read_buffer_;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionPortForwarding);
};

} // namespace host

#endif // HOST_CLIENT_SESSION_PORT_FORWARDING_H
