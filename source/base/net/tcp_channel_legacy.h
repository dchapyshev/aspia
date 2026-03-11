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

#ifndef BASE_NET_TCP_CHANNEL_LEGACY_H
#define BASE_NET_TCP_CHANNEL_LEGACY_H

#include <QByteArray>
#include <QQueue>
#include <QTimer>

#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>

#include "base/net/tcp_channel.h"
#include "base/peer/authenticator.h"

namespace base {

class Location;
class MessageEncryptor;
class MessageDecryptor;
class TcpServer;

class TcpChannelLegacy final : public TcpChannel
{
    Q_OBJECT

public:
    // Constructor available for client.
    explicit TcpChannelLegacy(Authenticator* authenticator, QObject* parent = nullptr);
    ~TcpChannelLegacy() final;

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
    friend class TcpServerLegacy;
    friend class RelayPeer;

    // Constructor available for server. An already connected socket is being moved.
    TcpChannelLegacy(asio::ip::tcp::socket&& socket, Authenticator* authenticator, QObject* parent);

    // Starts authentication. In the client channel, it starts automatically when a connection is
    // established. In the server channel, it is started by the RelayPeer or TcpServer.
    void doAuthentication() final;

    // Disconnects to remote host. The method is not available for an external call.
    // To disconnect, you must destroy the channel by calling the destructor.
    void disconnectFrom();

private:
    class VariableSizeReader
    {
    public:
        VariableSizeReader() = default;
        ~VariableSizeReader() = default;

        asio::mutable_buffer buffer();
        std::optional<size_t> messageSize();

    private:
        quint8 buffer_[4] = { 0 };
        size_t pos_ = 0;

        Q_DISABLE_COPY_MOVE(VariableSizeReader)
    };

    class VariableSizeWriter
    {
    public:
        VariableSizeWriter() = default;
        ~VariableSizeWriter() = default;

        asio::const_buffer variableSize(size_t size);

    private:
        quint8 buffer_[4];

        Q_DISABLE_COPY_MOVE(VariableSizeWriter)
    };

    class WriteTask
    {
    public:
        enum class Type { SERVICE_DATA, USER_DATA };

        WriteTask(Type type, quint8 channel_id, const QByteArray& data)
            : type_(type),
            channel_id_(channel_id),
            data_(data)
        {
            // Nothing
        }

        WriteTask(const WriteTask& other) = default;
        WriteTask& operator=(const WriteTask& other) = default;

        Type type() const { return type_; }
        quint8 channelId() const { return channel_id_; }
        const QByteArray& data() const { return data_; }
        QByteArray& data() { return data_; }

    private:
        Type type_;
        quint8 channel_id_;
        QByteArray data_;
    };

    enum class ReadState
    {
        IDLE,                // No reads are in progress right now.
        READ_SIZE,           // Reading the message size.
        READ_SERVICE_HEADER, // Reading the contents of the service header.
        READ_SERVICE_DATA,   // Reading the contents of the service data.
        READ_USER_DATA,      // Reading the contents of the user data.
        PENDING              // There is a message about which we did not notify.
    };

    struct UserDataHeader
    {
        quint8 channel_id;
        quint8 reserved;
    };

    enum ServiceMessageType
    {
        KEEP_ALIVE = 1
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

    struct ServiceHeader
    {
        quint8 type;      // Type of service packet (see ServiceDataType).
        quint8 flags;     // Flags bitmask (depends on the type).
        quint8 reserved1; // Reserved.
        quint8 reserved2; // Reserved.
        quint32 length;   // Additional data size.
    };

    void init();
    void setConnected(bool connected);

    void onKeyChanged();
    void onAuthenticatorMessage(const QByteArray& data);
    void onAuthenticatorFinished(Authenticator::ErrorCode error_code);

    void onErrorOccurred(const Location& location, const std::error_code& error_code);
    void onErrorOccurred(const Location& location, ErrorCode error_code);

    void onMessageWritten(quint8 channel_id);
    void onMessageReceived();

    void addWriteTask(WriteTask::Type type, quint8 channel_id, const QByteArray& data);

    void doWrite();
    void doReadSize();
    void doReadUserData(size_t length);
    void doReadServiceHeader();
    void doReadServiceData(size_t length);

    void onKeepAliveTimer();
    void sendKeepAlive(quint8 flags, const void* data, size_t size);

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
    VariableSizeWriter variable_size_writer_;
    QByteArray write_buffer_;

    ReadState state_ = ReadState::IDLE;
    VariableSizeReader variable_size_reader_;
    QByteArray read_buffer_;
    QByteArray decrypt_buffer_;

    bool is_channel_id_supported_ = false;

    Q_DISABLE_COPY_MOVE(TcpChannelLegacy)
};

} // namespace base

#endif // BASE_NET_TCP_CHANNEL_LEGACY_H
