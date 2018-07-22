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

constexpr quint32 kMaxMessageSize = 16 * 1024 * 1024; // 16MB
constexpr qint64 kMaxWriteSize = 1200;

QByteArray createWriteBuffer(const QByteArray& message_buffer)
{
    uint32_t message_size = message_buffer.size();

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

NetworkChannel::NetworkChannel(ChannelType channel_type, QTcpSocket* socket, QObject* parent)
    : QObject(parent),
      channel_type_(channel_type),
      socket_(socket)
{
    Q_ASSERT(!socket_.isNull());

    socket_->setParent(this);

    if (channel_type_ == ClientChannel)
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
    return new NetworkChannel(ClientChannel, new QTcpSocket(), parent);
}

void NetworkChannel::connectToHost(const QString& address, int port)
{
    if (channel_type_ == ServerChannel)
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

void NetworkChannel::readMessage()
{
    Q_ASSERT(!read_required_);

    read_required_ = true;
    onReadyRead();
}

void NetworkChannel::writeMessage(int message_id, const QByteArray& buffer)
{
    if (buffer.isEmpty())
    {
        stop();
        return;
    }

    write_queue_.push_back(qMakePair(message_id, buffer));

    if (write_buffer_.isEmpty())
        scheduleWrite();
}

void NetworkChannel::stop()
{
    channel_state_ = NotConnected;

    if (socket_->state() != QTcpSocket::UnconnectedState)
    {
        socket_->abort();

        if (socket_->state() != QTcpSocket::UnconnectedState)
            socket_->waitForDisconnected();
    }
}

void NetworkChannel::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == pinger_timer_id_)
    {
        // If at the moment the message is sent, then skip ping.
        if (!write_buffer_.isEmpty())
            return;

        // Pinger sends 1 byte equal to zero.
        write_buffer_.resize(1);
        write_buffer_[0] = 0;

        socket_->write(write_buffer_);
    }
}

void NetworkChannel::onConnected()
{
    channel_state_ = Connected;

    // Disable the Nagle algorithm for the socket.
    socket_->setSocketOption(QTcpSocket::LowDelayOption, 1);

    if (channel_type_ == ServerChannel)
    {
        encryptor_.reset(new Encryptor(Encryptor::Mode::SERVER));

        // Start reading hello message.
        readMessage();
    }
    else
    {
        Q_ASSERT(channel_type_ == ClientChannel);
        encryptor_.reset(new Encryptor(Encryptor::Mode::CLIENT));

        write_buffer_ = createWriteBuffer(encryptor_->helloMessage());

        // Write hello message to server.
        socket_->write(write_buffer_);
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
    written_ += bytes;

    if (written_ < write_buffer_.size())
    {
        int64_t bytes_to_write = qMin(write_buffer_.size() - written_, kMaxWriteSize);
        socket_->write(write_buffer_.constData() + written_, bytes_to_write);
    }
    else
    {
        onMessageWritten();

        write_buffer_.clear();
        written_ = 0;
    }
}

void NetworkChannel::onReadyRead()
{
    if (!read_required_)
        return;

    int64_t current;

    for (;;)
    {
        if (!read_size_received_)
        {
            uint8_t byte;

            current = socket_->read(reinterpret_cast<char*>(&byte), sizeof(byte));
            if (current == sizeof(byte))
            {
                switch (read_)
                {
                    case 0:
                    {
                        // If the first byte is zero, then message ping is received.
                        // This message is ignored.
                        if (byte == 0)
                            continue;

                        read_size_ += byte & 0x7F;
                    }
                    break;

                    case 1:
                        read_size_ += (byte & 0x7F) << 7;
                        break;

                    case 2:
                        read_size_ += (byte & 0x7F) << 14;
                        break;

                    case 3:
                        read_size_ += byte << 21;
                        break;
                }

                if (!(byte & 0x80) || read_ == 3)
                {
                    read_size_received_ = true;

                    if (!read_size_ || read_size_ > kMaxMessageSize)
                    {
                        qWarning() << "Wrong message size: " << read_size_;
                        stop();
                        return;
                    }

                    if (read_buffer_.capacity() < read_size_)
                        read_buffer_.reserve(read_size_);

                    read_buffer_.resize(read_size_);
                    read_size_ = 0;
                    read_ = 0;
                    continue;
                }
            }
        }
        else if (read_ < read_buffer_.size())
        {
            current = socket_->read(read_buffer_.data() + read_, read_buffer_.size() - read_);
        }
        else
        {
            read_required_ = false;
            read_size_received_ = false;
            read_ = 0;

            onMessageReceived();
            break;
        }

        if (current == 0)
            break;

        read_ += current;
    }
}

void NetworkChannel::onMessageWritten()
{
    switch (channel_state_)
    {
        case Encrypted:
        {
            // A message with a size of 1 byte can only be a ping.
            if (write_buffer_.size() != 1)
            {
                int message_id = write_queue_.front().first;
                if (message_id != -1)
                    emit messageWritten(message_id);

                write_queue_.pop_front();
            }

            if (!write_queue_.isEmpty())
                scheduleWrite();
        }
        break;

        case Connected:
        {
            if (channel_type_ == ServerChannel)
            {
                Q_ASSERT(!pinger_timer_id_);

                channel_state_ = Encrypted;
                pinger_timer_id_ = startTimer(std::chrono::seconds(30));

                emit connected();
            }
            else
            {
                Q_ASSERT(channel_type_ == ClientChannel);

                // Read hello message from server.
                readMessage();
            }
        }
        break;

        default:
            break;
    }
}

void NetworkChannel::onMessageReceived()
{
    switch (channel_state_)
    {
        case Encrypted:
        {
            if (!encryptor_)
            {
                stop();
                return;
            }

            int decrypted_data_size = encryptor_->decryptedDataSize(read_buffer_.size());

            if (decrypt_buffer_.capacity() < decrypted_data_size)
                decrypt_buffer_.reserve(decrypted_data_size);

            decrypt_buffer_.resize(decrypted_data_size);

            if (!encryptor_->decrypt(read_buffer_.constData(),
                                     read_buffer_.size(),
                                     decrypt_buffer_.data()))
            {
                stop();
                return;
            }

            emit messageReceived(decrypt_buffer_);
        }
        break;

        case Connected:
        {
            if (!encryptor_->readHelloMessage(read_buffer_))
            {
                stop();
                return;
            }

            if (channel_type_ == ServerChannel)
            {
                // Write hello message to client.
                socket_->write(createWriteBuffer(encryptor_->helloMessage()));
            }
            else
            {
                Q_ASSERT(channel_type_ == ClientChannel);
                Q_ASSERT(!pinger_timer_id_);

                channel_state_ = Encrypted;
                pinger_timer_id_ = startTimer(std::chrono::seconds(30));

                emit connected();
            }
        }
        break;

        default:
            break;
    }
}

void NetworkChannel::scheduleWrite()
{
    // Get the following message to send from the queue.
    const QByteArray& source_buffer = write_queue_.front().second;

    // Calculate the size of the encrypted message.
    int encrypted_data_size = encryptor_->encryptedDataSize(source_buffer.size());
    if (encrypted_data_size > kMaxMessageSize)
    {
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
    if (write_buffer_.capacity() < total_size)
        write_buffer_.reserve(total_size);

    // Change the size of the buffer.
    write_buffer_.resize(total_size);

    // Copy the size of the message to the buffer.
    memcpy(write_buffer_.data(), length_data, length_data_size);

    // Encrypt the message.
    if (!encryptor_->encrypt(source_buffer.constData(),
                             source_buffer.size(),
                             write_buffer_.data() + length_data_size))
    {
        stop();
        return;
    }

    // Send the buffer to the recipient.
    socket_->write(write_buffer_);
}

} // namespace aspia
