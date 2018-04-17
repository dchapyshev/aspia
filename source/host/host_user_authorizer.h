//
// PROJECT:         Aspia
// FILE:            host/host_user_authorizer.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_USER_AUTHORIZER_H
#define _ASPIA_HOST__HOST_USER_AUTHORIZER_H

#include <QObject>
#include <QScopedPointer>

#include "host/user_list.h"
#include "protocol/authorization.pb.h"

namespace aspia {

class Channel;

class HostUserAuthorizer : public QObject
{
    Q_OBJECT

public:
    HostUserAuthorizer(QObject* parent = nullptr);
    ~HostUserAuthorizer();

    void setUserList(const UserList& user_list);
    void setChannel(Channel* channel);

public slots:
    void start();
    void abort();

signals:
    void started();
    void finished();
    void createSession(proto::auth::SessionType session_type, Channel* channel);

protected:
    // QObject implementation.
    void timerEvent(QTimerEvent* event) override;

private slots:
    void messageWritten(int message_id);
    void readMessage(const QByteArray& buffer);

private:
    UserList user_list_;
    QScopedPointer<Channel> channel_;

    QByteArray nonce_;
    int timer_id_ = 0;

    proto::auth::SessionType session_type_ = proto::auth::SESSION_TYPE_UNKNOWN;
    proto::auth::Status status_ = proto::auth::STATUS_ACCESS_DENIED;

    Q_DISABLE_COPY(HostUserAuthorizer)
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_USER_AUTHORIZER_H
