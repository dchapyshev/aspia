//
// PROJECT:         Aspia
// FILE:            client/client.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_H
#define _ASPIA_CLIENT__CLIENT_H

#include "client/client_session.h"
#include "network/channel.h"
#include "protocol/computer.pb.h"

namespace aspia {

class ClientUserAuthorizer;
class StatusDialog;

class Client : public QObject
{
    Q_OBJECT

public:
    Client(const proto::Computer& computer, QObject* parent = nullptr);
    ~Client() = default;

signals:
    void clientTerminated();

private slots:
    void onChannelConnected();
    void onChannelDisconnected();
    void onChannelError(const QString& message);
    void authorizationFinished(proto::auth::Status status);

private:
    QPointer<Channel> channel_;
    QPointer<StatusDialog> status_dialog_;
    QPointer<ClientUserAuthorizer> authorizer_;
    QPointer<ClientSession> session_;

    proto::Computer computer_;

    Q_DISABLE_COPY(Client)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_H
