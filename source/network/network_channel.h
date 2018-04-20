//
// PROJECT:         Aspia
// FILE:            network/network_channel.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_CHANNEL_H
#define _ASPIA_NETWORK__NETWORK_CHANNEL_H

#include <QPair>
#include <QPointer>
#include <QQueue>
#include <QSslSocket>

namespace aspia {

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

    ~NetworkChannel();

    static NetworkChannel* createClient(QObject* parent = nullptr);

    void connectToHost(const QString& address, int port);

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
    void onEncrypted();
    void onError(QAbstractSocket::SocketError error);
    void onSslErrors(const QList<QSslError> &errors);
    void onBytesWritten(qint64 bytes);
    void onReadyRead();

private:
    friend class NetworkServer;

    NetworkChannel(ChannelType channel_type, QSslSocket* socket, QObject* parent);

    void scheduleWrite();

    using MessageSizeType = quint32;

    const ChannelType channel_type_;
    QPointer<QSslSocket> socket_;

    QQueue<QPair<int, QByteArray>> write_queue_;
    MessageSizeType write_size_ = 0;
    qint64 written_ = 0;

    bool read_required_ = false;
    bool read_size_received_ = false;
    QByteArray read_buffer_;
    MessageSizeType read_size_ = 0;
    qint64 read_ = 0;

    Q_DISABLE_COPY(NetworkChannel)
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_CHANNEL_H
