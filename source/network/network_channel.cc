//
// PROJECT:         Aspia
// FILE:            network/network_channel.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_channel.h"

#include <QtEndian>

#include "crypto/encryptor.h"

namespace aspia {

namespace {

constexpr quint32 kMaxMessageSize = 16 * 1024 * 1024; // 16MB
constexpr int kReadBufferReservedSize = 8 * 1024; // 8kB
constexpr int kWriteQueueReservedSize = 64;

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

    connect(socket_, &QTcpSocket::disconnected, this, &NetworkChannel::disconnected);
    connect(socket_, &QTcpSocket::bytesWritten, this, &NetworkChannel::onBytesWritten);
    connect(socket_, &QTcpSocket::readyRead, this, &NetworkChannel::onReadyRead);

    connect(socket_, QOverload<QTcpSocket::SocketError>::of(&QTcpSocket::error),
            this, &NetworkChannel::onError);

    read_buffer_.reserve(kReadBufferReservedSize);
    write_queue_.reserve(kWriteQueueReservedSize);
}

NetworkChannel::~NetworkChannel()
{
    stop();
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

    if (socket_.isNull())
        return;

    socket_->connectToHost(address, port);
}

void NetworkChannel::readMessage()
{
    Q_ASSERT(!read_required_);

    read_required_ = true;
    onReadyRead();
}

void NetworkChannel::writeMessage(int message_id, const QByteArray& buffer)
{
    if (encryptor_.isNull())
    {
        qWarning("Uninitialized encryptor");
        return;
    }

    write(message_id, encryptor_->encrypt(buffer));
}

void NetworkChannel::stop()
{
    channel_state_ = NotConnected;

    if (!socket_.isNull())
    {
        socket_->abort();

        if (socket_->state() != QTcpSocket::UnconnectedState)
            socket_->waitForDisconnected();
    }
}

void NetworkChannel::onConnected()
{
    channel_state_ = Connected;

    // Disable the Nagle algorithm for the socket.
    socket_->setSocketOption(QTcpSocket::LowDelayOption, 1);

    if (channel_type_ == ServerChannel)
    {
        encryptor_.reset(new Encryptor(Encryptor::ServerMode));

        // Start reading hello message.
        readMessage();
    }
    else
    {
        Q_ASSERT(channel_type_ == ClientChannel);
        encryptor_.reset(new Encryptor(Encryptor::ClientMode));

        // Write hello message to server.
        write(-1, encryptor_->helloMessage());
    }
}

void NetworkChannel::onError(QAbstractSocket::SocketError /* error */)
{
    emit errorOccurred(socket_->errorString());
}

void NetworkChannel::onBytesWritten(qint64 bytes)
{
    if (socket_.isNull())
    {
        stop();
        return;
    }

    written_ += bytes;

    const QByteArray& write_buffer = write_queue_.front().second;

    if (written_ < sizeof(MessageSizeType))
    {
        socket_->write(reinterpret_cast<const char*>(&write_size_) + written_,
                       sizeof(MessageSizeType) - written_);
    }
    else if (written_ < sizeof(MessageSizeType) + write_buffer.size())
    {
        socket_->write(write_buffer.data() + (written_ - sizeof(MessageSizeType)),
                       write_buffer.size() - (written_ - sizeof(MessageSizeType)));
    }
    else
    {
        onMessageWritten(write_queue_.front().first);

        write_queue_.pop_front();
        written_ = 0;

        if (!write_queue_.empty())
            scheduleWrite();
    }
}

void NetworkChannel::onReadyRead()
{
    if (socket_.isNull())
    {
        stop();
        return;
    }

    if (!read_required_)
        return;

    qint64 current;

    for (;;)
    {
        if (read_ < sizeof(MessageSizeType))
        {
            current = socket_->read(reinterpret_cast<char*>(&read_size_) + read_,
                                    sizeof(MessageSizeType) - read_);
        }
        else if (!read_size_received_ && read_ == sizeof(MessageSizeType))
        {
            read_size_received_ = true;

            read_size_ = qFromBigEndian(read_size_);
            if (!read_size_ || read_size_ > kMaxMessageSize)
            {
                qWarning() << "Wrong message size: " << read_size_;
                stop();
                return;
            }

            read_buffer_.resize(read_size_);

            current = socket_->read(read_buffer_.data(), read_buffer_.size());
        }
        else if (read_ < sizeof(MessageSizeType) + read_buffer_.size())
        {
            current = socket_->read(read_buffer_.data() + (read_ - sizeof(MessageSizeType)),
                                    read_buffer_.size() - (read_ - sizeof(MessageSizeType)));
        }
        else
        {
            read_required_ = false;
            read_size_received_ = false;
            current = read_ = 0;

            onMessageReceived(read_buffer_);
            break;
        }

        if (current == 0)
            break;

        read_ += current;
    }
}

void NetworkChannel::onMessageWritten(int message_id)
{
    switch (channel_state_)
    {
        case Encrypted:
            emit messageWritten(message_id);
            break;

        case Connected:
        {
            if (channel_type_ == ServerChannel)
            {
                channel_state_ = Encrypted;
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
    }
}

void NetworkChannel::onMessageReceived(const QByteArray& buffer)
{
    switch (channel_state_)
    {
        case Encrypted:
        {
            if (encryptor_.isNull())
            {
                qWarning("Uninitialized encryptor");
                return;
            }

            emit messageReceived(encryptor_->decrypt(buffer));
        }
        break;

        case Connected:
        {
            if (!encryptor_->readHelloMessage(buffer))
            {
                stop();
                return;
            }

            if (channel_type_ == ServerChannel)
            {
                write(-1, encryptor_->helloMessage());
            }
            else
            {
                Q_ASSERT(channel_type_ == ClientChannel);

                channel_state_ = Encrypted;
                emit connected();
            }
        }
        break;
    }
}

void NetworkChannel::write(int message_id, const QByteArray& buffer)
{
    if (socket_.isNull())
    {
        stop();
        return;
    }

    bool schedule_write = write_queue_.empty();

    write_queue_.push_back(QPair<int, QByteArray>(message_id, buffer));

    if (schedule_write)
        scheduleWrite();
}

void NetworkChannel::scheduleWrite()
{
    const QByteArray& write_buffer = write_queue_.front().second;

    write_size_ = static_cast<MessageSizeType>(write_buffer.size());
    if (!write_size_ || write_size_ > kMaxMessageSize)
    {
        stop();
        return;
    }

    write_size_ = qToBigEndian(write_size_);
    socket_->write(reinterpret_cast<const char*>(&write_size_), sizeof(MessageSizeType));
}

} // namespace aspia
