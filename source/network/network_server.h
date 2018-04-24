//
// PROJECT:         Aspia
// FILE:            network/network_server.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_SERVER_H
#define _ASPIA_NETWORK__NETWORK_SERVER_H

#include <QPointer>
#include <QQueue>
#include <QTcpServer>

namespace aspia {

class NetworkChannel;

class NetworkServer : public QObject
{
    Q_OBJECT

public:
    explicit NetworkServer(QObject* parent = nullptr);
    ~NetworkServer();

    bool start(int port);
    void stop();

    bool hasPendingChannels() const;
    NetworkChannel* nextPendingChannel();

signals:
    void newChannelConnected();

private slots:
    void onNewConnection();

private:
    QPointer<QTcpServer> tcp_server_;
    QQueue<NetworkChannel*> pending_channels_;

    Q_DISABLE_COPY(NetworkServer)
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_SERVER_H
