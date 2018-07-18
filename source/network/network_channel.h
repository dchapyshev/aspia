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

#ifndef _ASPIA_NETWORK__NETWORK_CHANNEL_H
#define _ASPIA_NETWORK__NETWORK_CHANNEL_H

#include <QPointer>
#include <QTcpSocket>

#include <queue>
#include <utility>

namespace aspia {

class Encryptor;
class NetworkServer;

class NetworkChannel : public QObject
{
    Q_OBJECT

public:
    enum ChannelType
    {
        ServerChannel,
        ClientChannel
    };
    Q_ENUM(ChannelType);

    enum ChannelState
    {
        NotConnected,
        Connected,
        Encrypted
    };
    Q_ENUM(ChannelState);

    ~NetworkChannel() = default;

    static NetworkChannel* createClient(QObject* parent = nullptr);

    void connectToHost(const QString& address, int port);

    ChannelState channelState() const { return channel_state_; }
    QString peerAddress() const;

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
    void messageReceived(const QByteArray& buffer);
    void messageWritten(int message_id);

public slots:
    // Starts reading the message. When the message is received, the signal |messageReceived| is
    // called. You do not need to re-call |readMessage| until this signal is called.
    void readMessage();

    // Sends a message. If the |message_id| is not -1, then after the message is sent,
    // the signal |messageWritten| is called.
    void writeMessage(int message_id, const QByteArray& buffer);

    // Stops the channel.
    void stop();

protected:
    void timerEvent(QTimerEvent* event) override;

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onBytesWritten(qint64 bytes);
    void onReadyRead();
    void onMessageWritten(int message_id);
    void onMessageReceived(const QByteArray& buffer);

private:
    friend class NetworkServer;

    NetworkChannel(ChannelType channel_type, QTcpSocket* socket, QObject* parent);

    void write(int message_id, const QByteArray& buffer);
    void scheduleWrite();

    using MessageSizeType = quint32;

    const ChannelType channel_type_;
    ChannelState channel_state_ = NotConnected;
    QPointer<QTcpSocket> socket_;

    std::unique_ptr<Encryptor> encryptor_;

    std::queue<std::pair<int, QByteArray>> write_queue_;
    qint64 written_ = 0;

    bool read_required_ = false;
    bool read_size_received_ = false;
    QByteArray read_buffer_;
    int read_size_ = 0;
    qint64 read_ = 0;

    int pinger_timer_id_ = 0;

    Q_DISABLE_COPY(NetworkChannel)
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_CHANNEL_H
