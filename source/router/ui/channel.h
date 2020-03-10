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

#ifndef ROUTER__UI__CHANNEL_H
#define ROUTER__UI__CHANNEL_H

#include "base/macros_magic.h"

#include <QQueue>
#include <QTcpSocket>

namespace router {

class Channel : public QObject
{
    Q_OBJECT

public:
    explicit Channel(QObject* parent = nullptr);
    ~Channel();

    void connectToRouter(
        const QString& address, uint16_t port, const QString& username, const QString& password);
    void send(const QByteArray& buffer);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
    void messageReceived(const QByteArray& buffer);

private slots:
    void onError(QAbstractSocket::SocketError error);
    void onConnected();
    void onBytesWritten(int64_t bytes);
    void onReadyRead();

private:
    void scheduleWrite();

    QTcpSocket* socket_ = nullptr;

    QString username_;
    QString password_;

    uint32_t read_pos_ = 0;
    uint32_t read_size_ = 0;
    QByteArray read_buffer_;

    uint32_t write_pos_ = 0;
    uint32_t write_size_ = 0;
    QQueue<QByteArray> write_queue_;

    DISALLOW_COPY_AND_ASSIGN(Channel);
};

} // namespace router

#endif // ROUTER__UI__CHANNEL_H
