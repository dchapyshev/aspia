//
// PROJECT:         Aspia
// FILE:            host/host_user_authorizer.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_USER_AUTHORIZER_H
#define _ASPIA_HOST__HOST_USER_AUTHORIZER_H

#include <QObject>
#include <QPointer>

#include "host/user_list.h"
#include "protocol/authorization.pb.h"

namespace aspia {

class NetworkChannel;

class HostUserAuthorizer : public QObject
{
    Q_OBJECT

public:
    enum State
    {
        NotStarted,
        RequestWrite,
        WaitForResponse,
        ResultWrite,
        Finished
    };

    HostUserAuthorizer(QObject* parent = nullptr);
    ~HostUserAuthorizer();

    void setUserList(const UserList& user_list);
    void setNetworkChannel(NetworkChannel* network_channel);

    NetworkChannel* networkChannel();
    proto::auth::Status status() const;
    proto::auth::SessionType sessionType() const;

public slots:
    void start();
    void stop();

signals:
    void finished(HostUserAuthorizer* authorizer);
    void writeMessage(int message_id, const QByteArray& buffer);
    void readMessage();

protected:
    // QObject implementation.
    void timerEvent(QTimerEvent* event) override;

private slots:
    void messageWritten(int message_id);
    void messageReceived(const QByteArray& buffer);

private:
    State state_ = NotStarted;

    UserList user_list_;
    QPointer<NetworkChannel> network_channel_;

    QByteArray nonce_;
    int timer_id_ = 0;

    proto::auth::SessionType session_type_ = proto::auth::SESSION_TYPE_UNKNOWN;
    proto::auth::Status status_ = proto::auth::STATUS_ACCESS_DENIED;

    Q_DISABLE_COPY(HostUserAuthorizer)
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_USER_AUTHORIZER_H
