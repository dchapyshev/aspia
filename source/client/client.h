//
// PROJECT:         Aspia
// FILE:            client/client.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_H
#define _ASPIA_CLIENT__CLIENT_H

#include "client/client_session.h"
#include "client/connect_data.h"
#include "network/network_channel.h"

namespace aspia {

class ClientUserAuthorizer;
class StatusDialog;

class Client : public QObject
{
    Q_OBJECT

public:
    Client(const ConnectData& connect_data, QObject* parent = nullptr);
    ~Client() = default;

signals:
    void clientTerminated(Client* client);

private slots:
    void onChannelConnected();
    void onChannelDisconnected();
    void onChannelError(const QString& message);
    void onAuthorizationFinished(proto::auth::Status status);
    void onSessionClosedByUser();
    void onSessionError(const QString& message);

private:
    ConnectData connect_data_;

    QPointer<NetworkChannel> network_channel_;
    QPointer<StatusDialog> status_dialog_;
    QPointer<ClientUserAuthorizer> authorizer_;
    QPointer<ClientSession> session_;

    Q_DISABLE_COPY(Client)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_H
