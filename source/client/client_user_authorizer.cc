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

#include "client/client_user_authorizer.h"

#include <QCryptographicHash>

#include "base/message_serialization.h"
#include "client/ui/authorization_dialog.h"
#include "crypto/secure_memory.h"

namespace aspia {

namespace {

const int kKeyHashingRounds = 100000;
const int kPasswordHashingRounds = 100000;

QByteArray createPasswordHash(const QString& password)
{
    QByteArray data = password.toUtf8();

    for (int i = 0; i < kPasswordHashingRounds; ++i)
    {
        data = QCryptographicHash::hash(data, QCryptographicHash::Sha512);
    }

    return data;
}

QByteArray createSessionKey(const QByteArray& password_hash, const QByteArray& nonce)
{
    QByteArray data = password_hash;

    for (int i = 0; i < kKeyHashingRounds; ++i)
    {
        QCryptographicHash hash(QCryptographicHash::Sha512);

        hash.addData(data);
        hash.addData(nonce);

        data = hash.result();
    }

    return data;
}

} // namespace

ClientUserAuthorizer::ClientUserAuthorizer(QWidget* parent)
    : QObject(parent)
{
    // Nothing
}

ClientUserAuthorizer::~ClientUserAuthorizer()
{
    secureMemZero(&username_);
    secureMemZero(&password_);

    cancel();
}

void ClientUserAuthorizer::setSessionType(proto::auth::SessionType session_type)
{
    if (state_ != State::NOT_STARTED)
    {
        qDebug("An attempt to set a session type in an already running authorizer");
        return;
    }

    session_type_ = session_type;
}

void ClientUserAuthorizer::setUserName(const QString& username)
{
    if (state_ != State::NOT_STARTED)
    {
        qDebug("An attempt to set a user name in an already running authorizer");
        return;
    }

    username_ = username;
}

void ClientUserAuthorizer::setPassword(const QString& password)
{
    if (state_ != State::NOT_STARTED)
    {
        qDebug("An attempt to set a password in an already running authorizer");
        return;
    }

    password_ = password;
}

void ClientUserAuthorizer::start()
{
    if (state_ != State::NOT_STARTED)
    {
        qDebug("Authorizer already started");
        return;
    }

    state_ = State::STARTED;
    writeLogonRequest();
    emit started();
}

void ClientUserAuthorizer::cancel()
{
    if (state_ == State::FINISHED)
        return;

    state_ = State::FINISHED;
    emit finished(proto::auth::STATUS_CANCELED);
}

void ClientUserAuthorizer::messageReceived(const QByteArray& buffer)
{
    if (state_ == State::FINISHED)
        return;

    proto::auth::HostToClient message;

    if (parseMessage(buffer, message))
    {
        if (message.has_server_challenge())
        {
            readServerChallenge(message.server_challenge());
            return;
        }
        else if (message.has_logon_result())
        {
            readLogonResult(message.logon_result());
            return;
        }
    }

    emit errorOccurred(tr("Protocol error: Unknown message from host."));
    cancel();
}

void ClientUserAuthorizer::writeLogonRequest()
{
    proto::auth::ClientToHost message;

    // We do not support other authorization methods yet.
    message.mutable_logon_request()->set_method(proto::auth::METHOD_BASIC);
    emit sendMessage(serializeMessage(message));
}

void ClientUserAuthorizer::readServerChallenge(
    const proto::auth::ServerChallenge& server_challenge)
{
    QByteArray nonce = QByteArray::fromStdString(server_challenge.nonce());
    if (nonce.isEmpty())
    {
        emit errorOccurred(tr("Authorization error: Empty nonce is not allowed."));
        cancel();
        return;
    }

    if (username_.isEmpty() || password_.isEmpty())
    {
        AuthorizationDialog dialog(dynamic_cast<QWidget*>(parent()));

        dialog.setUserName(username_);
        dialog.setPassword(password_);

        if (dialog.exec() == AuthorizationDialog::Rejected)
        {
            emit errorOccurred(tr("Authorization is canceled by the user."));
            cancel();
            return;
        }

        username_ = dialog.userName();
        password_ = dialog.password();
    }

    QByteArray password_hash = createPasswordHash(password_);
    QByteArray session_key = createSessionKey(password_hash, nonce);

    proto::auth::ClientToHost message;

    proto::auth::ClientChallenge* client_challenge = message.mutable_client_challenge();
    client_challenge->set_session_type(session_type_);
    client_challenge->set_username(username_.toStdString());
    client_challenge->set_session_key(session_key.constData(), session_key.size());

    secureMemZero(&password_hash);
    secureMemZero(&session_key);

    QByteArray serialized_message = serializeMessage(message);

    secureMemZero(client_challenge->mutable_username());
    secureMemZero(client_challenge->mutable_session_key());

    emit sendMessage(serialized_message);

    secureMemZero(&serialized_message);
    secureMemZero(&nonce);
}

void ClientUserAuthorizer::readLogonResult(const proto::auth::LogonResult& logon_result)
{
    if (logon_result.status() != proto::auth::STATUS_SUCCESS)
    {
        // Previous data for authorization is not correct. We clear the password so that the
        // authorization dialog is invoked.
        secureMemZero(&password_);
        password_.clear();

        // If the authorization fails, we send the authorization request again.
        writeLogonRequest();
    }
    else
    {
        state_ = State::FINISHED;
        emit finished(logon_result.status());
    }
}

} // namespace aspia
