//
// PROJECT:         Aspia
// FILE:            network/network_server.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_SERVER_H
#define _ASPIA_NETWORK__NETWORK_SERVER_H

#include <QPointer>
#include <QList>
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

    bool hasReadyChannels() const;
    NetworkChannel* nextReadyChannel();

signals:
    void newChannelReady();

private slots:
    void onNewConnection();
    void onChannelReady();

private:
    QPointer<QTcpServer> tcp_server_;

    // Contains a list of channels that are already connected, but the key exchange
    // is not yet complete.
    QList<QPointer<NetworkChannel>> pending_channels_;

    // Contains a list of channels that are ready for use.
    QList<QPointer<NetworkChannel>> ready_channels_;

    Q_DISABLE_COPY(NetworkServer)
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_SERVER_H
