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

#include "network/network_channel.h"

#include <QHostAddress>
#include <QNetworkProxy>
#include <QTimerEvent>

#include "crypto/encryptor.h"

namespace aspia {

namespace {

constexpr uint32_t kMaxMessageSize = 16 * 1024 * 1024; // 16 MB
constexpr int64_t kMaxWriteSize = 1200; // 1200 bytes
constexpr int kPingerInterval = 30; // 30 seconds

QByteArray createWriteBuffer(const QByteArray& message_buffer)
{
    uint32_t message_size = message_buffer.size();
    if (!message_size || message_size > kMaxMessageSize)
        return QByteArray();

    uint8_t length_data[4];
    int length_data_size = 1;

    length_data[0] = message_size & 0x7F;
    if (message_size > 0x7F) // 127 bytes
    {
        length_data[0] |= 0x80;
        length_data[length_data_size++] = message_size >> 7 & 0x7F;

        if (message_size > 0x3FFF) // 16383 bytes
        {
            length_data[1] |= 0x80;
            length_data[length_data_size++] = message_size >> 14 & 0x7F;

            if (message_size > 0x1FFFF) // 2097151 bytes
            {
                length_data[2] |= 0x80;
                length_data[length_data_size++] = message_size >> 21 & 0xFF;
            }
        }
    }

    QByteArray write_buffer;
    write_buffer.resize(length_data_size + message_size);

    memcpy(write_buffer.data(), length_data, length_data_size);
    memcpy(write_buffer.data() + length_data_size, message_buffer.constData(), message_size);

    return write_buffer;
}

} // namespace

NetworkChannel::NetworkChannel(Type channel_type, QTcpSocket* socket, QObject* parent)
    : QObject(parent),
      type_(channel_type),
      socket_(socket)
{
    Q_ASSERT(!socket_.isNull());

    socket_->setParent(this);

    if (type_ == Type::CLIENT_CHANNEL)
        connect(socket_, &QTcpSocket::connected, this, &NetworkChannel::onConnected);

    connect(socket_, &QTcpSocket::bytesWritten, this, &NetworkChannel::onBytesWritten);
    connect(socket_, &QTcpSocket::readyRead, this, &NetworkChannel::onReadyRead);

    connect(socket_, &QTcpSocket::disconnected,
            this, &NetworkChannel::onDisconnected,
            Qt::QueuedConnection);

    connect(socket_, QOverload<QTcpSocket::SocketError>::of(&QTcpSocket::error),
            this, &NetworkChannel::onError,
            Qt::QueuedConnection);
}

// static
NetworkChannel* NetworkChannel::createClient(QObject* parent)
{
    return new NetworkChannel(Type::CLIENT_CHANNEL, new QTcpSocket(), parent);
}

void NetworkChannel::connectToHost(const QString& address, int port)
{
    if (type_ == Type::SERVER_CHANNEL)
    {
        qWarning("The channel is server. The method invocation is invalid.");
        return;
    }

    socket_->setProxy(QNetworkProxy::NoProxy);
    socket_->connectToHost(address, port);
}

QString NetworkChannel::peerAddress() const
{
    QHostAddress address = socket_->peerAddress();

    bool ok = false;
    QHostAddress ipv4_address(address.toIPv4Address(&ok));
    if (ok)
        return ipv4_address.toString();

    return address.toString();
}

void NetworkChannel::start()
{
    if (isStarted())
        return;

    read_.paused = false;

    // Start receiving messages.
    onReadyRead();
}

void NetworkChannel::stop()
{
    state_ = State::NOT_CONNECTED;

    if (socket_->state() != QTcpSocket::UnconnectedState)
    {
        socket_->abort();

        if (socket_->state() != QTcpSocket::UnconnectedState)
            socket_->waitForDisconnected();
    }
}

void NetworkChannel::pause()
{
    read_.paused = true;
}

void NetworkChannel::send(const QByteArray& buffer)
{
    if (buffer.isEmpty())
    {
        emit errorOccurred("An attempt was made to send an empty message.");
        stop();
        return;
    }

    // If the write buffer is empty, then no write operation is currently performed.
    if (write_.buffer.isEmpty() && write_.queue.isEmpty())
    {
        // Start the write operation.
        scheduleWrite(buffer);
    }
    else
    {
        // Add the buffer to the queue for sending.
        write_.queue.push_back(buffer);
    }
}

void NetworkChannel::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == pinger_timer_id_)
    {
        // If at the moment the message is sent, then skip ping.
        if (!write_.buffer.isEmpty())
            return;

        // Pinger sends 1 byte equal to zero.
        write_.buffer.resize(1);
        write_.buffer[0] = 0;

        socket_->write(write_.buffer);
    }
}

void NetworkChannel::onConnected()
{
    state_ = State::CONNECTED;

    // Disable the Nagle algorithm for the socket.
    socket_->setSocketOption(QTcpSocket::LowDelayOption, 1);

    if (type_ == Type::SERVER_CHANNEL)
    {
        encryptor_.reset(new Encryptor(Encryptor::Mode::SERVER));

        // Start reading hello message.
        onReadyRead();
    }
    else
    {
        Q_ASSERT(type_ == Type::CLIENT_CHANNEL);
        encryptor_.reset(new Encryptor(Encryptor::Mode::CLIENT));

        write_.buffer = createWriteBuffer(encryptor_->helloMessage());
        if (write_.buffer.isEmpty())
        {
            emit errorOccurred(tr("Error in encryption key exchange."));
            stop();
            return;
        }

        // Write hello message to server.
        socket_->write(write_.buffer);
    }
}

void NetworkChannel::onDisconnected()
{
    if (pinger_timer_id_)
    {
        killTimer(pinger_timer_id_);
        pinger_timer_id_ = 0;
    }

    emit disconnected();
}

void NetworkChannel::onError(QAbstractSocket::SocketError /* error */)
{
    emit errorOccurred(socket_->errorString());
}

void NetworkChannel::onBytesWritten(int64_t bytes)
{
    write_.bytes_transferred += bytes;

    if (write_.bytes_transferred < write_.buffer.size())
    {
        int64_t bytes_to_write =
            qMin(write_.buffer.size() - write_.bytes_transferred, kMaxWriteSize);

        socket_->write(write_.buffer.constData() + write_.bytes_transferred, bytes_to_write);
    }
    else
    {
        onMessageWritten();

        write_.buffer.clear();
        write_.bytes_transferred = 0;
    }
}

void NetworkChannel::onReadyRead()
{
    if (read_.paused)
        return;

    int64_t current = 0;

    for (;;)
    {
        if (!read_.buffer_size_received)
        {
            uint8_t byte;

            current = socket_->read(reinterpret_cast<char*>(&byte), sizeof(byte));
            if (current == sizeof(byte))
            {
                switch (read_.bytes_transferred)
                {
                    case 0:
                    {
                        // If the first byte is zero, then message ping is received.
                        // This message is ignored.
                        if (!byte)
                            continue;

                        read_.buffer_size += byte & 0x7F;
                    }
                    break;

                    case 1:
                        read_.buffer_size += (byte & 0x7F) << 7;
                        break;

                    case 2:
                        read_.buffer_size += (byte & 0x7F) << 14;
                        break;

                    case 3:
                        read_.buffer_size += byte << 21;
                        break;
                }

                if (!(byte & 0x80) || read_.bytes_transferred == 3)
                {
                    read_.buffer_size_received = true;

                    if (!read_.buffer_size || read_.buffer_size > kMaxMessageSize)
                    {
                        emit errorOccurred(tr("The received message has an invalid size."));
                        stop();
                        return;
                    }

                    if (read_.buffer.capacity() < read_.buffer_size)
                        read_.buffer.reserve(read_.buffer_size);

                    read_.buffer.resize(read_.buffer_size);
                    read_.buffer_size = 0;
                    read_.bytes_transferred = 0;
                    continue;
                }
            }
        }
        else if (read_.bytes_transferred < read_.buffer.size())
        {
            current = socket_->read(read_.buffer.data() + read_.bytes_transferred,
                                    read_.buffer.size() - read_.bytes_transferred);
        }
        else
        {
            read_.buffer_size_received = false;
            read_.bytes_transferred = 0;

            onMessageReceived();
            return;
        }

        if (current <= 0)
            return;

        read_.bytes_transferred += current;
    }
}

void NetworkChannel::onMessageWritten()
{
    switch (state_)
    {
        case State::ENCRYPTED:
        {
            if (!write_.queue.isEmpty())
            {
                scheduleWrite(write_.queue.front());
                write_.queue.pop_front();
            }
        }
        break;

        case State::CONNECTED:
        {
            if (type_ == Type::SERVER_CHANNEL)
            {
                keyExchangeComplete();
            }
            else
            {
                Q_ASSERT(type_ == Type::CLIENT_CHANNEL);

                // Read hello message from server.
                onReadyRead();
            }
        }
        break;

        default:
            break;
    }
}

void NetworkChannel::onMessageReceived()
{
    switch (state_)
    {
        case State::ENCRYPTED:
        {
            if (!encryptor_)
            {
                emit errorOccurred(tr("Unknown internal error."));
                stop();
                return;
            }

            int decrypted_data_size = encryptor_->decryptedDataSize(read_.buffer.size());

            if (decrypt_buffer_.capacity() < decrypted_data_size)
                decrypt_buffer_.reserve(decrypted_data_size);

            decrypt_buffer_.resize(decrypted_data_size);

            if (!encryptor_->decrypt(read_.buffer.constData(),
                                     read_.buffer.size(),
                                     decrypt_buffer_.data()))
            {
                emit errorOccurred(tr("Error while decrypting the message."));
                stop();
                return;
            }

            emit messageReceived(decrypt_buffer_);

            // Read next message.
            onReadyRead();
        }
        break;

        case State::CONNECTED:
        {
            if (!encryptor_->readHelloMessage(read_.buffer))
            {
                emit errorOccurred(tr("Error in encryption key exchange."));
                stop();
                return;
            }

            if (type_ == Type::SERVER_CHANNEL)
            {
                // Write hello message to client.
                write_.buffer = createWriteBuffer(encryptor_->helloMessage());
                if (write_.buffer.isEmpty())
                {
                    emit errorOccurred(tr("Error in encryption key exchange."));
                    stop();
                    return;
                }

                socket_->write(write_.buffer);
            }
            else
            {
                Q_ASSERT(type_ == Type::CLIENT_CHANNEL);
                keyExchangeComplete();
            }
        }
        break;

        default:
            break;
    }
}

void NetworkChannel::keyExchangeComplete()
{
    Q_ASSERT(!pinger_timer_id_);

    pinger_timer_id_ = startTimer(std::chrono::seconds(kPingerInterval));
    if (!pinger_timer_id_)
    {
        emit errorOccurred(tr("Unknown internal error."));
        stop();
        return;
    }

    // After the successful completion of the key exchange, we pause the channel.
    // To continue receiving messages, slot |start| must be called.
    read_.paused = true;

    // The data channel is now encrypted.
    state_ = State::ENCRYPTED;
    emit connected();
}

void NetworkChannel::scheduleWrite(const QByteArray& source_buffer)
{
    // Calculate the size of the encrypted message.
    int encrypted_data_size = encryptor_->encryptedDataSize(source_buffer.size());
    if (encrypted_data_size > kMaxMessageSize)
    {
        emit errorOccurred(tr("The message to send exceeds the size limit."));
        stop();
        return;
    }

    uint8_t length_data[4];
    int length_data_size = 1;

    // Calculate the variable-length.
    length_data[0] = encrypted_data_size & 0x7F;
    if (encrypted_data_size > 0x7F) // 127 bytes
    {
        length_data[0] |= 0x80;
        length_data[length_data_size++] = encrypted_data_size >> 7 & 0x7F;

        if (encrypted_data_size > 0x3FFF) // 16383 bytes
        {
            length_data[1] |= 0x80;
            length_data[length_data_size++] = encrypted_data_size >> 14 & 0x7F;

            if (encrypted_data_size > 0x1FFFF) // 2097151 bytes
            {
                length_data[2] |= 0x80;
                length_data[length_data_size++] = encrypted_data_size >> 21 & 0xFF;
            }
        }
    }

    // Now we can calculate the full size.
    int total_size = length_data_size + encrypted_data_size;

    // If the reserved buffer size is less, then increase it.
    if (write_.buffer.capacity() < total_size)
        write_.buffer.reserve(total_size);

    // Change the size of the buffer.
    write_.buffer.resize(total_size);

    // Copy the size of the message to the buffer.
    memcpy(write_.buffer.data(), length_data, length_data_size);

    // Encrypt the message.
    if (!encryptor_->encrypt(source_buffer.constData(),
                             source_buffer.size(),
                             write_.buffer.data() + length_data_size))
    {
        emit errorOccurred(tr("Error while encrypting the message."));
        stop();
        return;
    }

    // Send the buffer to the recipient.
    socket_->write(write_.buffer);
}

} // namespace aspia
