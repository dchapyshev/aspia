//
// PROJECT:         Aspia
// FILE:            host/ipc_channel.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/ipc_channel.h"

#include <QDebug>
#include <QLocalSocket>

namespace aspia {

namespace {

constexpr quint32 kMaxMessageSize = 16 * 1024 * 1024; // 16MB
constexpr int kReadBufferReservedSize = 8 * 1024; // 8kB
constexpr int kWriteQueueReservedSize = 64;

} // namespace

IpcChannel::IpcChannel(QLocalSocket* socket, QObject* parent)
    : QObject(parent),
      socket_(socket)
{
    Q_ASSERT(socket_);

    socket_->setParent(this);

    connect(socket_, &QLocalSocket::connected, this, &IpcChannel::connected);
    connect(socket_, &QLocalSocket::disconnected, this, &IpcChannel::disconnected);
    connect(socket_, &QLocalSocket::bytesWritten, this, &IpcChannel::onBytesWritten);
    connect(socket_, &QLocalSocket::readyRead, this, &IpcChannel::onReadyRead);

    connect(socket_, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error),
            [this](QLocalSocket::LocalSocketError /* socket_error */)
    {
        qWarning() << "IPC channel error: " << socket_->errorString();
        emit errorOccurred();
    });

    read_buffer_.reserve(kReadBufferReservedSize);
    write_queue_.reserve(kWriteQueueReservedSize);
}

IpcChannel::~IpcChannel()
{
    socket_->abort();
}

// static
IpcChannel* IpcChannel::createClient(QObject* parent)
{
    return new IpcChannel(new QLocalSocket(), parent);
}

void IpcChannel::connectToServer(const QString& channel_name)
{
    socket_->connectToServer(channel_name);
}

void IpcChannel::disconnectFromServer()
{
    socket_->disconnectFromServer();
}

void IpcChannel::readMessage()
{
    Q_ASSERT(!read_required_);

    read_required_ = true;
    onReadyRead();
}

void IpcChannel::writeMessage(int message_id, const QByteArray& buffer)
{
    bool schedule_write = write_queue_.empty();

    write_queue_.push_back(QPair<int, QByteArray>(message_id, buffer));

    if (schedule_write)
        scheduleWrite();
}

void IpcChannel::onBytesWritten(qint64 bytes)
{
    const QByteArray& write_buffer = write_queue_.front().second;

    written_ += bytes;

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

void IpcChannel::onReadyRead()
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

void IpcChannel::scheduleWrite()
{
    const QByteArray& write_buffer = write_queue_.front().second;

    write_size_ = write_buffer.size();
    if (!write_size_ || write_size_ > kMaxMessageSize)
    {
        qWarning() << "Wrong message size: " << write_size_;
        socket_->abort();
        return;
    }

    socket_->write(reinterpret_cast<const char*>(&write_size_), sizeof(MessageSizeType));
}

} // namespace aspia
