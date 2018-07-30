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

#ifndef ASPIA_NETWORK__NETWORK_CHANNEL_H_
#define ASPIA_NETWORK__NETWORK_CHANNEL_H_

#include <QPointer>
#include <QQueue>
#include <QTcpSocket>

#include "base/macros_magic.h"

namespace aspia {

class Encryptor;
class NetworkServer;

class NetworkChannel : public QObject
{
    Q_OBJECT

public:
    enum class Type { SERVER_CHANNEL, CLIENT_CHANNEL };
    enum class State { NOT_CONNECTED, CONNECTED, ENCRYPTED };

    ~NetworkChannel() = default;

    // Creates a client to connect to the host.
    static NetworkChannel* createClient(QObject* parent = nullptr);

    // Connection to the host. If the channel is server, it does nothing.
    void connectToHost(const QString& address, int port);

    // Returns the state of the data channel.
    State state() const { return state_; }

    // If the channel is started, it returns true, if not, then false.
    bool isStarted() const { return !read_.paused; }

    // Returns the address of the connected peer.
    QString peerAddress() const;

signals:
    // Emits when a secure connection is established.
    void connected();

    // Emits when the connection is aborted.
    void disconnected();

    // Emitted when an error occurred. Parameter |message| contains a text description of the error.
    void errorOccurred(const QString& message);

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
    void timerEvent(QTimerEvent* event) override;

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onBytesWritten(int64_t bytes);
    void onReadyRead();
    void onMessageWritten();
    void onMessageReceived();

private:
    friend class NetworkServer;
    NetworkChannel(Type channel_type, QTcpSocket* socket, QObject* parent);
    void keyExchangeComplete();
    void scheduleWrite(const QByteArray& source_buffer);

    const Type type_;
    State state_ = State::NOT_CONNECTED;
    QPointer<QTcpSocket> socket_;

    // Encrypts and decrypts data.
    QScopedPointer<Encryptor> encryptor_;

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

    int pinger_timer_id_ = 0;

    DISALLOW_COPY_AND_ASSIGN(NetworkChannel);
};

} // namespace aspia

#endif // ASPIA_NETWORK__NETWORK_CHANNEL_H_
