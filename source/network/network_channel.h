//
// PROJECT:         Aspia
// FILE:            network/network_channel.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
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

    ~NetworkChannel();

    static NetworkChannel* createClient(QObject* parent = nullptr);

    void connectToHost(const QString& address, int port);

    ChannelState channelState() const { return channel_state_; }

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
    void messageReceived(const QByteArray& buffer);
    void messageWritten(int message_id);

public slots:
    void readMessage();
    void writeMessage(int message_id, const QByteArray& buffer);
    void stop();

private slots:
    void onConnected();
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
    quint32 read_size_ = 0;
    qint64 read_ = 0;

    Q_DISABLE_COPY(NetworkChannel)
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_CHANNEL_H
