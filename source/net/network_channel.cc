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

#include "net/network_channel.h"

#include "base/logging.h"
#include "crypto/cryptor_fake.h"
#include "net/network_channel_proxy.h"
#include "net/network_listener.h"

#include <QNetworkProxy>

#if defined(OS_WIN)
#include <winsock2.h>
#include <mstcpip.h>
#endif // defined(OS_WIN)

namespace net {

namespace {

constexpr uint32_t kMaxMessageSize = 16 * 1024 * 1024; // 16 MB
constexpr int64_t kMaxWriteSize = 1200; // 1200 bytes

void enableKeepAlive(QTcpSocket* socket)
{
#if defined(OS_WIN)
    struct tcp_keepalive alive;

    alive.onoff = 1; // On.
    alive.keepalivetime = 60000; // 60 seconds.
    alive.keepaliveinterval = 5000; // 5 seconds.

    DWORD bytes_returned;

    if (WSAIoctl(socket->socketDescriptor(), SIO_KEEPALIVE_VALS,
                 &alive, sizeof(alive), nullptr, 0, &bytes_returned,
                 nullptr, nullptr) == SOCKET_ERROR)
    {
        PLOG(LS_WARNING) << "WSAIoctl failed";
    }
#else
#warning Not implemented
#endif
}

void disableNagle(QTcpSocket* socket)
{
    socket->setSocketOption(QTcpSocket::LowDelayOption, 1);
}

} // namespace

Channel::Channel()
    : socket_(new QTcpSocket(this))
{
    init();
}

Channel::Channel(QTcpSocket* socket)
    : socket_(socket)
{
    socket_->setParent(this);

    disableNagle(socket_);
    enableKeepAlive(socket_);

    is_connected_ = true;

    init();
}

Channel::~Channel()
{
    proxy_->willDestroyCurrentChannel();
    proxy_ = nullptr;
}

void Channel::connectToHost(const QString& address, uint16_t port)
{
    connect(socket_, &QTcpSocket::connected, [this]()
    {
        disableNagle(socket_);
        enableKeepAlive(socket_);

        is_connected_ = true;

        if (listener_)
            listener_->onNetworkConnected();
    });

    socket_->setProxy(QNetworkProxy::NoProxy);
    socket_->connectToHost(address, port);
}

void Channel::disconnectFromHost()
{
    if (!is_connected_)
        return;

    is_connected_ = false;

    socket_->abort();
    socket_->waitForDisconnected();
}

void Channel::setListener(Listener* listener)
{
    listener_ = listener;
}

void Channel::setCryptor(std::unique_ptr<crypto::Cryptor> cryptor)
{
    cryptor_ = std::move(cryptor);
}

void Channel::pause()
{
    is_paused_ = true;
}

void Channel::resume()
{
    if (!is_connected_ || !is_paused_)
        return;

    is_paused_ = false;

    // Start receiving messages.
    onReadyRead();
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

void Channel::send(const QByteArray& buffer)
{
    if (buffer.isEmpty())
    {
        errorOccurred(ErrorCode::UNKNOWN);
        return;
    }

    bool schedule_write = write_.queue.empty();

    // Add the buffer to the queue for sending.
    write_.queue.emplace(buffer);

    if (schedule_write)
        scheduleWrite();
}

void Channel::onSocketError(QAbstractSocket::SocketError error)
{
    ErrorCode error_code;

    switch (error)
    {
        case QAbstractSocket::ConnectionRefusedError:
            error_code = ErrorCode::CONNECTION_REFUSED;
            break;

        case QAbstractSocket::RemoteHostClosedError:
            error_code = ErrorCode::REMOTE_HOST_CLOSED;
            break;

        case QAbstractSocket::HostNotFoundError:
            error_code = ErrorCode::SPECIFIED_HOST_NOT_FOUND;
            break;

        case QAbstractSocket::SocketTimeoutError:
            error_code = ErrorCode::SOCKET_TIMEOUT;
            break;

        case QAbstractSocket::AddressInUseError:
            error_code = ErrorCode::ADDRESS_IN_USE;
            break;

        case QAbstractSocket::SocketAddressNotAvailableError:
            error_code = ErrorCode::ADDRESS_NOT_AVAILABLE;
            break;

        case QAbstractSocket::NetworkError:
            error_code = ErrorCode::NETWORK_ERROR;
            break;

        default:
            LOG(LS_WARNING) << "Unhandled value of Qt error: " << error;
            error_code = ErrorCode::UNKNOWN;
            break;
    }

    errorOccurred(error_code);
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
    if (is_paused_)
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
                        errorOccurred(ErrorCode::UNKNOWN);
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
    DCHECK(!write_.queue.empty());

    // Delete the sent message from the queue.
    write_.queue.pop();

    // If the queue is not empty, then we send the following message.
    if (!write_.queue.empty())
        scheduleWrite();
}

void Channel::onMessageReceived()
{
    int decrypted_data_size = cryptor_->decryptedDataSize(read_.buffer.size());

    if (decrypt_buffer_.capacity() < decrypted_data_size)
        decrypt_buffer_.reserve(decrypted_data_size);

    decrypt_buffer_.resize(decrypted_data_size);

    if (!cryptor_->decrypt(read_.buffer.constData(),
                           read_.buffer.size(),
                           decrypt_buffer_.data()))
    {
        errorOccurred(ErrorCode::UNKNOWN);
        return;
    }

    if (listener_)
        listener_->onNetworkMessage(decrypt_buffer_);

    // Read next message.
    onReadyRead();
}

void Channel::init()
{
    proxy_.reset(new ChannelProxy(this));
    cryptor_.reset(new crypto::CryptorFake());

    connect(socket_, &QTcpSocket::bytesWritten, this, &Channel::onBytesWritten);
    connect(socket_, &QTcpSocket::readyRead, this, &Channel::onReadyRead);

    connect(socket_, QOverload<QTcpSocket::SocketError>::of(&QTcpSocket::error),
            this, &Channel::onSocketError);

    connect(socket_, &QTcpSocket::disconnected, [this]()
    {
        if (listener_)
        {
            listener_->onNetworkDisconnected();
            listener_ = nullptr;
        }
    });
}

void Channel::errorOccurred(ErrorCode error_code)
{
    if (listener_)
        listener_->onNetworkError(error_code);

    disconnectFromHost();
}

void Channel::scheduleWrite()
{
    const QByteArray& source_buffer = write_.queue.front();

    // Calculate the size of the encrypted message.
    size_t encrypted_data_size = cryptor_->encryptedDataSize(source_buffer.size());
    if (encrypted_data_size > kMaxMessageSize)
    {
        errorOccurred(ErrorCode::UNKNOWN);
        return;
    }

    uint8_t length_data[4];
    size_t length_data_size = 1;

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
        errorOccurred(ErrorCode::UNKNOWN);
        return;
    }

    // Send the buffer to the recipient.
    socket_->write(write_.buffer);
}

} // namespace net
