//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef ASPIA_CLIENT__CLIENT_USER_AUTHORIZER_H_
#define ASPIA_CLIENT__CLIENT_USER_AUTHORIZER_H_

#include <QObject>

#include "protocol/authorization.pb.h"

namespace aspia {

class ClientUserAuthorizer : public QObject
{
    Q_OBJECT

public:
    enum State { NotStarted, Started, Finished };

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
    void readServerChallenge(const proto::auth::ServerChallenge& server_challenge);
    void readLogonResult(const proto::auth::LogonResult& logon_result);

    State state_ = NotStarted;
    proto::auth::SessionType session_type_ = proto::auth::SESSION_TYPE_UNKNOWN;
    QString username_;
    QString password_;

    Q_DISABLE_COPY(ClientUserAuthorizer)
};

} // namespace aspia

#endif // ASPIA_CLIENT__CLIENT_USER_AUTHORIZER_H_
