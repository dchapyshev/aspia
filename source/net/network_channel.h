//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/macros_magic.h"
#include "net/network_error.h"

#if defined(USE_TBB)
#include <tbb/scalable_allocator.h>
#endif // defined(USE_TBB)

#include <QTcpSocket>

#include <queue>

namespace crypto {
class Cryptor;
} // namespace crypto

namespace net {

class ChannelProxy;
class Listener;

class Channel : public QObject
{
    Q_OBJECT

public:
    Channel();
    virtual ~Channel() = default;

    std::shared_ptr<ChannelProxy> channelProxy() { return proxy_; }

    // Connects to a host at the specified address and port.
    void connectToHost(const QString& address, uint16_t port);

    // Disconnects to remote host.
    void disconnectFromHost();

    // Sets an instance of the class to receive connection status notifications or new messages.
    // You can change this in the process.
    void setListener(Listener* listener);

    // Sets an instance of a class to encrypt and decrypt messages.
    // By default, a fake cryptographer is created that only copies the original message.
    // You must explicitly establish a cryptographer before or after establishing a connection.
    void setCryptor(std::unique_ptr<crypto::Cryptor> cryptor);

    // Returns true if the channel is connected and false if not connected.
    bool isConnected() const { return is_connected_; }

    // Returns true if the channel is paused and false if not. If the channel is not connected,
    // then the return value is undefined.
    bool isPaused() const { return is_paused_; }

    // Pauses the channel. After calling the method, new messages will not be read from the socket.
    // If at the time the method was called, the message was read, then notification of this
    // message will be received only after calling method resume().
    void pause();

    // After calling the method, reading new messages will continue.
    void resume();

    // Returns the address of the connected peer.
    QString peerAddress() const;

    // Sends a message.
    void send(const QByteArray& buffer);

protected:
    friend class Server;
    explicit Channel(QTcpSocket* socket);

private slots:
    void onSocketError(QAbstractSocket::SocketError error);
    void onBytesWritten(int64_t bytes);
    void onReadyRead();
    void onMessageWritten();
    void onMessageReceived();

private:
    void init();
    void errorOccurred(ErrorCode error_code);
    void scheduleWrite();

    std::shared_ptr<ChannelProxy> proxy_;
    Listener* listener_ = nullptr;

    QTcpSocket* socket_ = nullptr;

    // Encrypts and decrypts data.
    std::unique_ptr<crypto::Cryptor> cryptor_;

    bool is_connected_ = false;
    bool is_paused_ = true;

    // To this buffer decrypts the data received from the network.
    QByteArray decrypt_buffer_;

    struct WriteContext
    {
#if defined(USE_TBB)
        using QueueAllocator = tbb::scalable_allocator<QByteArray>;
#else // defined(USE_TBB)
        using QueueAllocator = std::allocator<QByteArray>;
#endif // defined(USE_*)

        using QueueContainer = std::deque<QByteArray, QueueAllocator>;

        // The queue contains unencrypted source messages.
        std::queue<QByteArray, QueueContainer> queue;

        // The buffer contains an encrypted message that is being sent to the current moment.
        QByteArray buffer;

        // Number of bytes transferred from the |buffer|.
        int64_t bytes_transferred = 0;
    };

    struct ReadContext
    {
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
