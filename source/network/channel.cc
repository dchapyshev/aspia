//
// PROJECT:         Aspia
// FILE:            network/channel.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/channel.h"

#include <QtEndian>

namespace aspia {

namespace {

constexpr quint32 kMaxMessageSize = 16 * 1024 * 1024; // 16MB
constexpr int kReadBufferReservedSize = 8 * 1024; // 8kB

} // namespace

Channel::Channel(QTcpSocket* socket, QObject* parent)
    : QObject(parent),
      socket_(socket)
{
    Q_ASSERT(socket_);

    socket_->setParent(this);

    connect(socket_, &QTcpSocket::connected, this, &Channel::onConnected);
    connect(socket_, &QTcpSocket::disconnected, this, &Channel::disconnected);
    connect(socket_, &QTcpSocket::bytesWritten, this, &Channel::onBytesWritten);
    connect(socket_, &QTcpSocket::readyRead, this, &Channel::onReadyRead);
    connect(socket_, QOverload<QTcpSocket::SocketError>::of(&QTcpSocket::error),
            this, &Channel::onError);

    read_buffer_.reserve(kReadBufferReservedSize);
}

Channel::~Channel()
{
    socket_->abort();
}

// static
Channel* Channel::createClient(QObject* parent)
{
    return new Channel(new QTcpSocket(), parent);
}

void Channel::connectToHost(const QString& address, int port)
{
    socket_->connectToHost(address, port);
}

void Channel::readMessage()
{
    Q_ASSERT(!read_required_);

    read_required_ = true;
    onReadyRead();
}

void Channel::writeMessage(int message_id, const QByteArray& buffer)
{
    bool schedule_write = write_queue_.empty();

    write_queue_.push_back(QPair<int, QByteArray>(message_id, buffer));

    if (schedule_write)
        scheduleWrite();
}

void Channel::stop()
{
    socket_->abort();
}

void Channel::onConnected()
{
    socket_->setSocketOption(QTcpSocket::LowDelayOption, 1);
    emit connected();
}

void Channel::onError(QAbstractSocket::SocketError /* error */)
{
    emit errorOccurred(socket_->errorString());
}

void Channel::onBytesWritten(qint64 bytes)
{
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
        emit messageWritten(write_queue_.front().first);

        write_queue_.pop_front();
        written_ = 0;

        if (!write_queue_.empty())
            scheduleWrite();
    }
}

void Channel::onReadyRead()
{
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
                socket_->abort();
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

            emit messageReceived(read_buffer_);
            break;
        }

        if (current == 0)
            break;

        read_ += current;
    }
}

void Channel::scheduleWrite()
{
    const QByteArray& write_buffer = write_queue_.front().second;

    write_size_ = static_cast<MessageSizeType>(write_buffer.size());
    if (!write_size_ || write_size_ > kMaxMessageSize)
    {
        socket_->abort();
        return;
    }

    write_size_ = qToBigEndian(write_size_);
    socket_->write(reinterpret_cast<const char*>(&write_size_), sizeof(MessageSizeType));
}

} // namespace aspia
