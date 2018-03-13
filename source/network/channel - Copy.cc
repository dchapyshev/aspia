//
// PROJECT:         Aspia
// FILE:            network/channel.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/channel.h"

#include <QApplication>
#include <QtEndian>

namespace aspia {

namespace {

constexpr quint32 kMaxMessageSize = 10 * 1024 * 1024; // 10MB
constexpr int kReadBufferReservedSize = 4096; // 4kB

} // namespace

Channel::Channel(QObject* parent)
    : QTcpSocket(parent)
{
    connect(this, SIGNAL(bytesWritten(qint64)), this, SLOT(onBytesWritten(qint64)));
    connect(this, SIGNAL(readyRead()), this, SLOT(onReadyRead()));

    read_buffer_.reserve(kReadBufferReservedSize);
}

void Channel::writeMessage(const QByteArray& buffer)
{
    bool schedule_write = write_queue_.empty();

    write_queue_.push_back(buffer);

    if (schedule_write)
        scheduleWrite();
}

void Channel::stopChannel()
{
    disconnectFromHost();
}

void Channel::onBytesWritten(qint64 bytes)
{
    written_ += bytes;

    if (written_ < sizeof(MessageSizeType))
    {
        write(reinterpret_cast<const char*>(&write_size_) + written_,
              sizeof(MessageSizeType) - written_);
    }
    else if (written_ < sizeof(MessageSizeType) + write_queue_.front().size())
    {
        write(write_queue_.front().data() + (written_ - sizeof(MessageSizeType)),
              write_queue_.front().size() - (written_ - sizeof(MessageSizeType)));
    }
    else
    {
        write_queue_.pop_front();
        written_ = 0;

        if (!write_queue_.empty())
            scheduleWrite();
    }
}

void Channel::onReadyRead()
{
    qint64 current;

    for (;;)
    {
        if (read_ < sizeof(MessageSizeType))
        {
            current = read(reinterpret_cast<char*>(&read_size_) + read_,
                           sizeof(MessageSizeType) - read_);
        }
        else if (!read_size_received_ && read_ == sizeof(MessageSizeType))
        {
            read_size_received_ = true;

            read_size_ = qFromBigEndian(read_size_);
            if (!read_size_ || read_size_ > kMaxMessageSize)
            {
                qWarning() << "Wrong message size: " << read_size_;
                abort();
                return;
            }

            read_buffer_.resize(read_size_);

            current = read(read_buffer_.data(), read_buffer_.size());
        }
        else if (read_ < sizeof(MessageSizeType) + read_buffer_.size())
        {
            current = read(read_buffer_.data() + (read_ - sizeof(MessageSizeType)),
                           read_buffer_.size() - (read_ - sizeof(MessageSizeType)));
        }
        else
        {
            read_size_received_ = false;
            current = read_ = 0;
            emit channelMessage(read_buffer_);
        }

        if (current == 0)
            break;

        read_ += current;
    }
}

void Channel::scheduleWrite()
{
    write_size_ = static_cast<MessageSizeType>(write_queue_.front().size());
    if (!write_size_ || write_size_ > kMaxMessageSize)
    {
        abort();
        return;
    }

    write_size_ = qToBigEndian(write_size_);
    write(reinterpret_cast<const char*>(&write_size_), sizeof(MessageSizeType));
}

} // namespace aspia
