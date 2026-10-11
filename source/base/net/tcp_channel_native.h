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

#ifndef BASE_NET_TCP_CHANNEL_NATIVE_H
#define BASE_NET_TCP_CHANNEL_NATIVE_H

#include <asio/ip/tcp.hpp>

#include "base/net/tcp_channel.h"

class TcpChannelNative final : public TcpChannel
{
    Q_OBJECT

public:
    // Constructor available for client.
    TcpChannelNative(Type type, Authenticator* authenticator, QObject* parent = nullptr);
    ~TcpChannelNative() final;

    // TcpChannel implementation.
    void connectTo(const QString& address, quint16 port, Seconds timeout) final;
    bool setReadBufferSize(int size) final;
    bool setWriteBufferSize(int size) final;

protected:
    friend class TcpServer;

    // Constructor available for server. An already connected socket is being moved.
    TcpChannelNative(Type type, asio::ip::tcp::socket&& socket, Authenticator* authenticator, QObject* parent);

    // TcpChannel implementation.
    void readTransport(void* data, size_t size) final;
    void writeTransport(const void* data, size_t size) final;
    void closeTransport() final;

private:
    void onConnected();

    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;
    std::unique_ptr<asio::ip::tcp::resolver> resolver_;

    Q_DISABLE_COPY_MOVE(TcpChannelNative)
};

#endif // BASE_NET_TCP_CHANNEL_NATIVE_H
