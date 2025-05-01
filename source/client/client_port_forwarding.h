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

#include "base/memory/local_memory.h"
#include "client/client.h"
#include "proto/port_forwarding.pb.h"

#include <queue>

#include <QPointer>
#include <QTimer>

namespace client {

class ClientPortForwarding final : public Client
{
    Q_OBJECT

public:
    explicit ClientPortForwarding(QObject* parent = nullptr);
    ~ClientPortForwarding() final;

    struct Statistics
    {
        quint64 rx_bytes = 0;
        quint64 tx_bytes = 0;
    };

    void setPortForwardingConfig(const proto::port_forwarding::Config& config);

signals:
    void sig_statistics(const client::ClientPortForwarding::Statistics& statistics);

protected:
    // Client implementation.
    void onSessionStarted() final;
    void onSessionMessageReceived(uint8_t channel_id, const QByteArray& buffer) final;
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

    State state_ = State::DISCONNECED;
    bool is_started_ = false;

    std::string remote_host_;
    quint16 remote_port_ = 0;
    quint16 local_port_ = 0;
    std::string command_line_;

    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    std::unique_ptr<asio::ip::tcp::socket> socket_;

    proto::port_forwarding::HostToClient incoming_message_;
    proto::port_forwarding::ClientToHost outgoing_message_;

    class Handler;
    base::local_shared_ptr<Handler> handler_;

    std::queue<std::string> write_queue_;

    static const int kBufferSize = 8192;
    std::array<char, kBufferSize> read_buffer_;

    QPointer<QTimer> statistics_timer_;
    quint64 rx_bytes_ = 0;
    quint64 tx_bytes_ = 0;
};

} // namespace client

#endif // CLIENT_CLIENT_PORT_FORWARDING_H
