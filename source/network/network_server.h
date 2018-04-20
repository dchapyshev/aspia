//
// PROJECT:         Aspia
// FILE:            network/network_server.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_SERVER_H
#define _ASPIA_NETWORK__NETWORK_SERVER_H

#include <QPointer>
#include <QQueue>

#include "network/ssl_server.h"

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
    void onNewSslConnection();

private:
    QPointer<SslServer> ssl_server_;
    QQueue<NetworkChannel*> pending_channels_;

    Q_DISABLE_COPY(NetworkServer)
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_SERVER_H
