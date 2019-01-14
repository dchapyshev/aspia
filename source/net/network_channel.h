//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef NET__NETWORK_CHANNEL_H
#define NET__NETWORK_CHANNEL_H

#include <QPointer>
#include <QQueue>
#include <QTcpSocket>
#include <QVersionNumber>

#include "base/macros_magic.h"

namespace crypto {
class Cryptor;
} // namespace crypto

namespace net {

class Channel : public QObject
{
    Q_OBJECT

public:
    enum class ChannelType { HOST, CLIENT };
    enum class ChannelState { NOT_CONNECTED, CONNECTED, ENCRYPTED };
    enum class KeyExchangeState { HELLO, IDENTIFY, KEY_EXCHANGE, SESSION, DONE };

    enum class Error
    {
        UNKNOWN,                  // Unknown error.
        CONNECTION_REFUSED,       // The connection was refused by the peer (or timed out).
        REMOTE_HOST_CLOSED,       // The remote host closed the connection.
        SPECIFIED_HOST_NOT_FOUND, // The host address was not found.
        SOCKET_TIMEOUT,           // The socket operation timed out.
        ADDRESS_IN_USE,           // The address specified is already in use and was set to be exclusive.
        ADDRESS_NOT_AVAILABLE,    // The address specified does not belong to the host.
        PROTOCOL_FAILURE,         // Violation of the data exchange protocol.
        ENCRYPTION_FAILURE,       // An error occurred while encrypting the message.
        DECRYPTION_FAILURE,       // An error occurred while decrypting the message.
        AUTHENTICATION_FAILURE,   // An error occured while authenticating.
        SESSION_TYPE_NOT_ALLOWED  // The specified session type is not allowed for the user.
    };

    virtual ~Channel() = default;

    // Returns the state of the data channel.
    ChannelState channelState() const { return channel_state_; }

    // If the channel is started, it returns true, if not, then false.
    bool isStarted() const { return !read_.paused; }

    // Returns the address of the connected peer.
    QString peerAddress() const;

    // Returns the version of the connected peer.
    QVersionNumber peerVersion() const;

signals:
    // Emits when the connection is aborted.
    void disconnected();

    // Emitted when an error occurred. Parameter |message| contains a text description of the error.
    void errorOccurred(Error error);

    // Emitted when a new message is received.
    void messageReceived(const QByteArray& buffer);

public slots:
    // Starts reading messages from the channel. After receiving each new message, the signal
    // |messageReceived| will be emmited.
    // If the channel is already started, it does nothing.
    void start();

    // Stops the channel. After calling this slot, new messages will not arrive and the sending of
    // messages will be stopped. All messages that are in the sending queue will be deleted.
    void stop();

    // Pauses channel. Receiving incoming messages is suspended. To continue data transfer, you
    // need to call slot |start|.
    void pause();

    // Sends a message.
    void send(const QByteArray& buffer);

protected:
    QPointer<QTcpSocket> socket_;
    QVersionNumber peer_version_;

    // Encrypts and decrypts data.
    std::unique_ptr<crypto::Cryptor> cryptor_;

    ChannelState channel_state_ = ChannelState::NOT_CONNECTED;
    KeyExchangeState key_exchange_state_ = KeyExchangeState::HELLO;

    Channel(ChannelType channel_type, QTcpSocket* socket, QObject* parent);

    void sendInternal(const QByteArray& buffer);

    virtual void internalMessageReceived(const QByteArray& buffer) = 0;
    virtual void internalMessageWritten() = 0;

private slots:
    void onError(QAbstractSocket::SocketError error);
    void onBytesWritten(int64_t bytes);
    void onReadyRead();
    void onMessageWritten();
    void onMessageReceived();

private:
    void scheduleWrite();

    const ChannelType channel_type_;

    // To this buffer decrypts the data received from the network.
    QByteArray decrypt_buffer_;

    struct WriteContext
    {
        // The queue contains unencrypted source messages.
        QQueue<QByteArray> queue;

        // The buffer contains an encrypted message that is being sent to the current moment.
        QByteArray buffer;

        // Number of bytes transferred from the |buffer|.
        int64_t bytes_transferred = 0;
    };

    struct ReadContext
    {
        bool paused = false;

        // To this buffer reads data from the network.
        QByteArray buffer;

        // If the flag is set to true, then the buffer size is read from the network, if false,
        // then no.
        bool buffer_size_received = false;

        // Size of |buffer|.
        int buffer_size = 0;

        // Number of bytes read into the |buffer|.
        int64_t bytes_transferred = 0;
    };

    ReadContext read_;
    WriteContext write_;

    DISALLOW_COPY_AND_ASSIGN(Channel);
};

} // namespace net

#endif // NET__NETWORK_CHANNEL_H
