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

#ifndef _ASPIA_IPC__IPC_CHANNEL_H
#define _ASPIA_IPC__IPC_CHANNEL_H

#include <QByteArray>
#include <QLocalSocket>
#include <QPointer>

#include <queue>
#include <utility>

namespace aspia {

class IpcServer;

class IpcChannel : public QObject
{
    Q_OBJECT

public:
    enum State
    {
        NotConnected,
        Connected
    };

    ~IpcChannel() = default;

    static IpcChannel* createClient(QObject* parent = nullptr);

    void connectToServer(const QString& channel_name);
    State channelState() const { return state_; }

public slots:
    void stop();

    // Starts reading the message. When the message is received, the signal |messageReceived| is
    // called. You do not need to re-call |readMessage| until this signal is called.
    void readMessage();

    // Sends a message. If the |message_id| is not -1, then after the message is sent,
    // the signal |messageWritten| is called.
    void writeMessage(int message_id, const QByteArray& buffer);

signals:
    void connected();
    void disconnected();
    void errorOccurred();
    void messageWritten(int message_id);
    void messageReceived(const QByteArray& buffer);

private slots:
    void onError(QLocalSocket::LocalSocketError socket_error);
    void onBytesWritten(qint64 bytes);
    void onReadyRead();

private:
    friend class IpcServer;

    IpcChannel(QLocalSocket* socket, QObject* parent);

    void scheduleWrite();

    using MessageSizeType = quint32;

    QPointer<QLocalSocket> socket_;
    State state_ = NotConnected;

    std::queue<std::pair<int, QByteArray>> write_queue_;
    MessageSizeType write_size_ = 0;
    qint64 written_ = 0;

    bool read_required_ = false;
    bool read_size_received_ = false;
    QByteArray read_buffer_;
    MessageSizeType read_size_ = 0;
    qint64 read_ = 0;

    Q_DISABLE_COPY(IpcChannel)
};

} // namespace aspia

#endif // _ASPIA_IPC__IPC_CHANNEL_H
