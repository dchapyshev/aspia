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

#ifndef BASE_NET_TCP_CHANNEL_NG_H
#define BASE_NET_TCP_CHANNEL_NG_H

#include <QQueue>

#include <asio/ip/tcp.hpp>

#include "base/net/tcp_channel.h"

class QTimer;

namespace base {

class Authenticator;
class Location;
class MessageEncryptor;
class MessageDecryptor;
class TcpServer;

class TcpChannelNG final : public TcpChannel
{
    Q_OBJECT

public:
    // Constructor available for client.
    explicit TcpChannelNG(Authenticator* authenticator, QObject* parent = nullptr);
    ~TcpChannelNG() final;

    // Gets the address of the remote host as a string.
    QString peerAddress() const final;

    // Connects to a host at the specified address and port.
    void connectTo(const QString& address, quint16 port) final;

    // Returns true if the channel is connected and false if not connected.
    bool isConnected() const final { return connected_; }

    // Returns true if the channel has already been fully authenticated.
    bool isAuthenticated() const final { return authenticated_; }

    // Returns true if the channel is paused and false if not. If the channel is not connected,
    // then the return value is undefined.
    bool isPaused() const final { return paused_; }

    // Pauses or resume the channel. After calling the method, new messages will not be read from
    // the socket. If at the time the method was called with |true', the message was read, then
    // notification of this message will be received only after calling method with |false|.
    void setPaused(bool enable) final;

    // Sending a message. After the call, the message will be added to the queue to be sent.
    void send(quint8 channel_id, const QByteArray& buffer) final;

    bool setReadBufferSize(int size) final;
    bool setWriteBufferSize(int size) final;

    qint64 pendingBytes() const final;

protected:
    friend class TcpServer;
    friend class RelayPeer;

    // Constructor available for server. An already connected socket is being moved.
    TcpChannelNG(asio::ip::tcp::socket&& socket, Authenticator* authenticator, QObject* parent);

    // Starts authentication. In the client channel, it starts automatically when a connection is
    // established. In the server channel, it is started by the RelayPeer or TcpServer.
    void doAuthentication() final;

private:
    class WriteTask
    {
    public:
        WriteTask(quint8 type, quint8 param, const QByteArray& data)
            : type_(type),
              param_(param),
              data_(data)
        {
            // Nothing
        }

        WriteTask(const WriteTask& other) = default;
        WriteTask& operator=(const WriteTask& other) = default;

        quint8 type() const { return type_; }
        quint8 param() const { return param_; }
        const QByteArray& data() const { return data_; }
        QByteArray& data() { return data_; }

    private:
        quint8 type_;
        quint8 param_;
        QByteArray data_;
    };

    enum class ReadState
    {
        IDLE,        // No reads are in progress right now.
        READ_HEADER, // Reading the contents of the header.
        READ_DATA,   // Reading the contents of the data.
        PENDING      // There is a message about which we did not notify.
    };

    enum MessageType
    {
        KEEP_ALIVE = 1,
        AUTH_DATA  = 2,
        USER_DATA  = 3
    };

    enum KeepAliveFlags
    {
        KEEP_ALIVE_PONG = 0,
        KEEP_ALIVE_PING = 1
    };

    enum KeepAliveTimerType
    {
        KEEP_ALIVE_TIMEOUT = 0,
        KEEP_ALIVE_INTERVAL = 1
    };

    struct Header
    {
        quint8 type;
        quint8 param1; // Depends on the message type.
        quint8 param2; // Not used yet.
        quint8 param3; // Not used yet.
        quint32 length;
    };

    void init();
    void setConnected(bool connected);
    void onErrorOccurred(const Location& location, const std::error_code& error_code);
    void onErrorOccurred(const Location& location, ErrorCode error_code);
    void onMessageReceived();
    void addWriteTask(quint8 type, quint8 param, const QByteArray& data);
    void doWrite();
    void doReadHeader();
    void doReadData();
    void onKeepAliveTimer();

    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;
    std::unique_ptr<asio::ip::tcp::resolver> resolver_;

    QTimer* keep_alive_timer_ = nullptr;
    KeepAliveTimerType keep_alive_timer_type_ = KEEP_ALIVE_INTERVAL;
    QByteArray keep_alive_counter_;

    bool connected_ = false;
    bool authenticated_ = false;
    bool paused_ = true;

    Authenticator* authenticator_ = nullptr;
    std::unique_ptr<MessageEncryptor> encryptor_;
    std::unique_ptr<MessageDecryptor> decryptor_;

    QQueue<WriteTask> write_queue_;
    QByteArray write_buffer_;

    ReadState state_ = ReadState::IDLE;
    Header read_header_;
    QByteArray read_buffer_;
    QByteArray decrypt_buffer_;

    Q_DISABLE_COPY_MOVE(TcpChannelNG)
};

} // namespace base

#endif // BASE_NET_TCP_CHANNEL_NG_H
