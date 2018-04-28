//
// PROJECT:         Aspia
// FILE:            client/client.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_H
#define _ASPIA_CLIENT__CLIENT_H

#include "client/client_session.h"
#include "network/network_channel.h"
#include "protocol/address_book.pb.h"

namespace aspia {

class ClientUserAuthorizer;
class StatusDialog;

class Client : public QObject
{
    Q_OBJECT

public:
    Client(const proto::address_book::Computer& computer, QObject* parent = nullptr);
    ~Client() = default;

signals:
    void clientTerminated();

private slots:
    void onChannelConnected();
    void onChannelDisconnected();
    void onChannelError(const QString& message);
    void authorizationFinished(proto::auth::Status status);

private:
    QPointer<NetworkChannel> network_channel_;
    QPointer<StatusDialog> status_dialog_;
    QPointer<ClientUserAuthorizer> authorizer_;
    QPointer<ClientSession> session_;

    proto::address_book::Computer computer_;

    Q_DISABLE_COPY(Client)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_H
