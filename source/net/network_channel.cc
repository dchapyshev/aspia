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

#include "net/network_channel.h"

#include <QNetworkProxy>

#include "base/logging.h"
#include "common/message_serialization.h"
#include "crypto/cryptor.h"

namespace net {

namespace {

constexpr uint32_t kMaxMessageSize = 16 * 1024 * 1024; // 16 MB
constexpr int64_t kMaxWriteSize = 1200; // 1200 bytes

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

Channel::Channel(ChannelType channel_type, QTcpSocket* socket, QObject* parent)
    : QObject(parent),
      channel_type_(channel_type),
      socket_(socket)
{
    DCHECK(!socket_.isNull());

    socket_->setParent(this);

    connect(socket_, &QTcpSocket::bytesWritten, this, &Channel::onBytesWritten);
    connect(socket_, &QTcpSocket::readyRead, this, &Channel::onReadyRead);

    connect(socket_, &QTcpSocket::disconnected, this, &Channel::disconnected,
            Qt::QueuedConnection);

    connect(socket_, QOverload<QTcpSocket::SocketError>::of(&QTcpSocket::error),
            this, &Channel::onError,
            Qt::QueuedConnection);

    connect(this, &Channel::errorOccurred, this, &Channel::stop);
}

QString Channel::peerAddress() const
{
    QHostAddress address = socket_->peerAddress();

    bool ok = false;
    QHostAddress ipv4_address(address.toIPv4Address(&ok));
    if (ok)
        return ipv4_address.toString();

    return address.toString();
}

QVersionNumber Channel::peerVersion() const
{
    return peer_version_;
}

void Channel::start()
{
    if (isStarted())
        return;

    read_.paused = false;

    // Start receiving messages.
    onReadyRead();
}

void Channel::stop()
{
    channel_state_ = ChannelState::NOT_CONNECTED;

    if (socket_->state() != QTcpSocket::UnconnectedState)
    {
        socket_->abort();

        if (socket_->state() != QTcpSocket::UnconnectedState)
            socket_->waitForDisconnected();
    }
}

void Channel::pause()
{
    read_.paused = true;
}

void Channel::send(const QByteArray& buffer)
{
    if (buffer.isEmpty())
    {
        emit errorOccurred(Error::UNKNOWN);
        return;
    }

    bool schedule_write = write_.queue.isEmpty();

    // Add the buffer to the queue for sending.
    write_.queue.push_back(buffer);

    if (schedule_write)
        scheduleWrite();
}

void Channel::sendInternal(const QByteArray& buffer)
{
    write_.buffer = createWriteBuffer(buffer);
    if (write_.buffer.isEmpty())
    {
        stop();
        return;
    }

    socket_->write(write_.buffer);
}

void Channel::onError(QAbstractSocket::SocketError error)
{
    Error channel_error;

    switch (error)
    {
        case QAbstractSocket::ConnectionRefusedError:
            channel_error = Error::CONNECTION_REFUSED;
            break;

        case QAbstractSocket::RemoteHostClosedError:
            channel_error = Error::REMOTE_HOST_CLOSED;
            break;

        case QAbstractSocket::HostNotFoundError:
            channel_error = Error::SPECIFIED_HOST_NOT_FOUND;
            break;

        case QAbstractSocket::SocketTimeoutError:
            channel_error = Error::SOCKET_TIMEOUT;
            break;

        case QAbstractSocket::AddressInUseError:
            channel_error = Error::ADDRESS_IN_USE;
            break;

        case QAbstractSocket::SocketAddressNotAvailableError:
            channel_error = Error::ADDRESS_NOT_AVAILABLE;
            break;

        default:
            channel_error = Error::UNKNOWN;
            break;
    }

    emit errorOccurred(channel_error);
}

void Channel::onBytesWritten(int64_t bytes)
{
    write_.bytes_transferred += bytes;

    if (write_.bytes_transferred < write_.buffer.size())
    {
        int64_t bytes_to_write =
            std::min(write_.buffer.size() - write_.bytes_transferred, kMaxWriteSize);

        socket_->write(write_.buffer.constData() + write_.bytes_transferred, bytes_to_write);
    }
    else
    {
        onMessageWritten();

        write_.buffer.clear();
        write_.bytes_transferred = 0;
    }
}

void Channel::onReadyRead()
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
                        read_.buffer_size += byte & 0x7F;
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

                    default:
                        break;
                }

                if (!(byte & 0x80) || read_.bytes_transferred == 3)
                {
                    read_.buffer_size_received = true;

                    if (!read_.buffer_size || read_.buffer_size > kMaxMessageSize)
                    {
                        emit errorOccurred(Error::UNKNOWN);
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

void Channel::onMessageWritten()
{
    if (channel_state_ == ChannelState::ENCRYPTED)
    {
        DCHECK(!write_.queue.empty());

        // Delete the sent message from the queue.
        write_.queue.pop_front();

        // If the queue is not empty, then we send the following message.
        if (!write_.queue.isEmpty())
            scheduleWrite();
    }
    else
    {
        internalMessageWritten();
    }
}

void Channel::onMessageReceived()
{
    if (channel_state_ == ChannelState::ENCRYPTED)
    {
        int decrypted_data_size = cryptor_->decryptedDataSize(read_.buffer.size());

        if (decrypt_buffer_.capacity() < decrypted_data_size)
            decrypt_buffer_.reserve(decrypted_data_size);

        decrypt_buffer_.resize(decrypted_data_size);

        if (!cryptor_->decrypt(read_.buffer.constData(),
                               read_.buffer.size(),
                               decrypt_buffer_.data()))
        {
            emit errorOccurred(Error::DECRYPTION_FAILURE);
            return;
        }

        emit messageReceived(decrypt_buffer_);
    }
    else
    {
        internalMessageReceived(read_.buffer);
    }

    // Read next message.
    onReadyRead();
}

void Channel::scheduleWrite()
{
    const QByteArray& source_buffer = write_.queue.front();

    // Calculate the size of the encrypted message.
    int encrypted_data_size = cryptor_->encryptedDataSize(source_buffer.size());
    if (encrypted_data_size > kMaxMessageSize)
    {
        emit errorOccurred(Error::UNKNOWN);
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
    if (!cryptor_->encrypt(source_buffer.constData(),
                           source_buffer.size(),
                           write_.buffer.data() + length_data_size))
    {
        emit errorOccurred(Error::ENCRYPTION_FAILURE);
        return;
    }

    // Send the buffer to the recipient.
    socket_->write(write_.buffer);
}

} // namespace net
