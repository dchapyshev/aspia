//
// PROJECT:         Aspia
// FILE:            network/server.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__SERVER_H
#define _ASPIA_NETWORK__SERVER_H

#include <QObject>
#include <QPointer>

#include "host/user_list.h"
#include "protocol/authorization.pb.h"

class QTcpServer;

namespace aspia {

class Channel;
class Host;
class HostUserAuthorizer;

class Server : public QObject
{
    Q_OBJECT

public:
    Server(QObject* parent = nullptr);
    ~Server();

    bool start(int port);
    void stop();
    void setSessionChanged(quint32 event, quint32 session_id);

signals:
    void sessionChanged(quint32 event, quint32 session_id);

private slots:
    void onNewConnection();
    void onAuthorizationFinished(HostUserAuthorizer* authorizer);
    void onHostFinished(Host* host);

private:
    UserList user_list_;
    QPointer<QTcpServer> server_;

    Q_DISABLE_COPY(Server)
};

} // namespace aspia

#endif // _ASPIA_NETWORK__SERVER_H
