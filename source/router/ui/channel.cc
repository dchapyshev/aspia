//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "router/ui/channel.h"

#include "base/endian.h"

#include <QNetworkProxy>

namespace router {

namespace {

const uint32_t kMaxMessageSize = 16 * 1024 * 1024; // 16MB

} // namespace

Channel::Channel(QObject* parent)
    : QObject(parent),
      socket_(new QTcpSocket(this))
{
    connect(socket_, &QTcpSocket::bytesWritten, this, &Channel::onBytesWritten);
    connect(socket_, &QTcpSocket::readyRead, this, &Channel::onReadyRead);
    connect(socket_, &QTcpSocket::connected, this, &Channel::onConnected);
    connect(socket_, &QTcpSocket::disconnected, this, &Channel::disconnected);
    connect(socket_, QOverload<QTcpSocket::SocketError>::of(&QTcpSocket::error),
            this, &Channel::onError);
}

Channel::~Channel() = default;

void Channel::connectToRouter(
    const QString& address, uint16_t port, const QString& username, const QString& password)
{
    username_ = username;
    password_ = password;

    socket_->setProxy(QNetworkProxy::NoProxy);
    socket_->connectToHost(address, port);
}

void Channel::send(const QByteArray& buffer)
{
    if (buffer.isEmpty() || buffer.size() > kMaxMessageSize)
    {
        emit errorOccurred(tr("Unknown internal error: message with incorrect size."));
        return;
    }

    bool schedule_write = write_queue_.empty();

    write_queue_.append(buffer);

    if (schedule_write)
        scheduleWrite();
}

void Channel::onError(QAbstractSocket::SocketError /* error */)
{
    emit errorOccurred(socket_->errorString());
}

void Channel::onConnected()
{

}

void Channel::onBytesWritten(int64_t bytes)
{
    const QByteArray& write_buffer = write_queue_.front();

    write_pos_ += bytes;

    if (write_pos_ < sizeof(uint32_t))
    {
        socket_->write(reinterpret_cast<const char*>(&write_size_) + write_pos_,
                       sizeof(uint32_t) - write_pos_);
    }
    else if (write_pos_ < sizeof(uint32_t) + write_buffer.size())
    {
        socket_->write(write_buffer.data() + (write_pos_ - sizeof(uint32_t)),
                       write_buffer.size() - (write_pos_ - sizeof(uint32_t)));
    }
    else
    {
        write_queue_.pop_front();
        write_pos_ = 0;
        write_size_ = 0;

        if (!write_queue_.empty())
            scheduleWrite();
    }
}

void Channel::onReadyRead()
{
    int64_t current = 0;

    for (;;)
    {
        if (read_pos_ < sizeof(uint32_t))
        {
            current = socket_->read(
                reinterpret_cast<char*>(&read_size_) + read_pos_, sizeof(uint32_t) - read_pos_);
        }
        else if (read_pos_ < read_size_ + sizeof(uint32_t))
        {
            uint32_t read_pos = read_pos_ - sizeof(uint32_t);

            current = socket_->read(read_buffer_.data() + read_pos, read_size_ - read_pos);
        }

        if (current <= 0)
            break;

        read_pos_ += current;

        if (read_pos_ == sizeof(uint32_t))
        {
            read_size_ = base::Endian::fromBig(read_size_);
            read_buffer_.resize(read_size_);
        }
        else if (read_pos_ == read_size_ + sizeof(uint32_t))
        {
            read_pos_ = 0;
            read_size_ = 0;

            emit messageReceived(read_buffer_);
        }
    }
}

void Channel::scheduleWrite()
{
    const QByteArray& write_buffer = write_queue_.front();

    write_size_ = write_buffer.size();
    if (!write_size_ || write_size_ > kMaxMessageSize)
    {
        emit errorOccurred(tr("Unknown internal error: message with incorrect size."));
        return;
    }

    write_size_ = base::Endian::toBig(write_size_);
    socket_->write(reinterpret_cast<const char*>(&write_size_), sizeof(uint32_t));
}

} // namespace router
