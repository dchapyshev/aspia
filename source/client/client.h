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
#include "proto/computer.pb.h"

namespace aspia {

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

    void onAuthorizationRequest(const QByteArray& buffer);
    void onAuthorizationResult(const QByteArray& buffer);

private:
    Channel* channel_;
    StatusDialog* status_dialog_;

    ClientSession* session_;

    proto::Computer computer_;

    DISALLOW_COPY_AND_ASSIGN(Client);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_H
