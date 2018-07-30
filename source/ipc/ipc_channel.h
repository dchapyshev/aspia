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

#ifndef ASPIA_IPC__IPC_CHANNEL_H_
#define ASPIA_IPC__IPC_CHANNEL_H_

#include <QByteArray>
#include <QLocalSocket>
#include <QQueue>
#include <QPointer>

#include "base/macros_magic.h"

namespace aspia {

class IpcServer;

class IpcChannel : public QObject
{
    Q_OBJECT

public:
    ~IpcChannel() = default;

    static IpcChannel* createClient(QObject* parent = nullptr);

    void connectToServer(const QString& channel_name);

public slots:
    void stop();

    // Starts reading the message.
    void start();

    // Sends a message.
    void send(const QByteArray& buffer);

signals:
    void connected();
    void disconnected();
    void errorOccurred();
    void messageReceived(const QByteArray& buffer);

private slots:
    void onError(QLocalSocket::LocalSocketError socket_error);
    void onBytesWritten(int64_t bytes);
    void onReadyRead();

private:
    friend class IpcServer;
    IpcChannel(QLocalSocket* socket, QObject* parent);

    void scheduleWrite();

    using MessageSizeType = uint32_t;

    QPointer<QLocalSocket> socket_;

    QQueue<QByteArray> write_queue_;
    MessageSizeType write_size_ = 0;
    int64_t written_ = 0;

    bool read_size_received_ = false;
    QByteArray read_buffer_;
    MessageSizeType read_size_ = 0;
    int64_t read_ = 0;

    DISALLOW_COPY_AND_ASSIGN(IpcChannel);
};

} // namespace aspia

#endif // ASPIA_IPC__IPC_CHANNEL_H_
