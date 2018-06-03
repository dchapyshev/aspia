//
// PROJECT:         Aspia
// FILE:            client/client_user_authorizer.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_USER_AUTHORIZER_H
#define _ASPIA_CLIENT__CLIENT_USER_AUTHORIZER_H

#include <QObject>

#include "protocol/authorization.pb.h"

namespace aspia {

class ClientUserAuthorizer : public QObject
{
    Q_OBJECT

public:
    enum State
    {
        NotStarted,
        WaitForRequest,
        ResponseWrite,
        WaitForResult,
        Finished
    };
    Q_ENUM(State);

    explicit ClientUserAuthorizer(QWidget* parent);
    ~ClientUserAuthorizer();

    proto::auth::SessionType sessionType() const { return session_type_; }
    void setSessionType(proto::auth::SessionType session_type);

    QString userName() const { return username_; }
    void setUserName(const QString& username);

    QString password() const { return password_; }
    void setPassword(const QString& password);

public slots:
    void start();
    void cancel();
    void messageWritten(int message_id);
    void messageReceived(const QByteArray& buffer);

signals:
    void finished(proto::auth::Status status);
    void errorOccurred(const QString& message);
    void writeMessage(int message_id, const QByteArray& buffer);
    void readMessage();

private:
    State state_ = NotStarted;
    proto::auth::SessionType session_type_ = proto::auth::SESSION_TYPE_UNKNOWN;
    QString username_;
    QString password_;
    QByteArray nonce_;

    Q_DISABLE_COPY(ClientUserAuthorizer)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_USER_AUTHORIZER_H
