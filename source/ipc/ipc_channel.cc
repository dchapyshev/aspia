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

#include "ipc/ipc_channel.h"

#include <QDebug>

namespace aspia {

namespace {

constexpr quint32 kMaxMessageSize = 16 * 1024 * 1024; // 16MB

} // namespace

IpcChannel::IpcChannel(QLocalSocket* socket, QObject* parent)
    : QObject(parent),
      socket_(socket)
{
    Q_ASSERT(socket_);

    socket_->setParent(this);

    connect(socket_, &QLocalSocket::connected, [this]()
    {
        state_ = Connected;
        emit connected();
    });

    connect(socket_, &QLocalSocket::bytesWritten, this, &IpcChannel::onBytesWritten);
    connect(socket_, &QLocalSocket::readyRead, this, &IpcChannel::onReadyRead);

    connect(socket_, &QLocalSocket::disconnected,
            this, &IpcChannel::disconnected,
            Qt::QueuedConnection);

    connect(socket_, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error),
            this, &IpcChannel::onError,
            Qt::QueuedConnection);
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

void IpcChannel::stop()
{
    state_ = NotConnected;

    if (socket_->state() != QLocalSocket::UnconnectedState)
    {
        socket_->abort();

        if (socket_->state() != QLocalSocket::UnconnectedState)
            socket_->waitForDisconnected();
    }
}

void IpcChannel::readMessage()
{
    Q_ASSERT(!read_required_);

    read_required_ = true;
    onReadyRead();
}

void IpcChannel::writeMessage(int message_id, const QByteArray& buffer)
{
    bool schedule_write = write_queue_.isEmpty();

    write_queue_.push_back(qMakePair(message_id, buffer));

    if (schedule_write)
        scheduleWrite();
}

void IpcChannel::onError(QLocalSocket::LocalSocketError /* socket_error */)
{
    qWarning() << "IPC channel error: " << socket_->errorString();
    emit errorOccurred();
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
        int message_id = write_queue_.front().first;

        if (message_id != -1)
            emit messageWritten(message_id);

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
        if (!read_size_received_)
        {
            current = socket_->read(reinterpret_cast<char*>(&read_size_) + read_,
                                    sizeof(MessageSizeType) - read_);
            if (current + read_ == sizeof(MessageSizeType))
            {
                read_size_received_ = true;

                if (!read_size_ || read_size_ > kMaxMessageSize)
                {
                    qWarning() << "Wrong message size: " << read_size_;
                    socket_->abort();
                    return;
                }

                if (read_buffer_.capacity() < static_cast<int>(read_size_))
                    read_buffer_.reserve(read_size_);

                read_buffer_.resize(read_size_);
                read_ = 0;
                continue;
            }
        }
        else if (read_ < read_size_)
        {
            current = socket_->read(read_buffer_.data() + read_, read_size_ - read_);
        }
        else
        {
            read_required_ = false;
            read_size_received_ = false;
            read_ = 0;

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
