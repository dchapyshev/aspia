//
// PROJECT:         Aspia
// FILE:            client/client_user_authorizer.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_user_authorizer.h"

#include <QCryptographicHash>
#include <QDebug>

#include "base/message_serialization.h"
#include "client/ui/authorization_dialog.h"
#include "crypto/secure_memory.h"

namespace aspia {

namespace {

const quint32 kKeyHashingRounds = 100000;
const quint32 kPasswordHashingRounds = 100000;

enum MessageId { LogonRequest, ClientChallenge };

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

    for (quint32 i = 0; i < kKeyHashingRounds; ++i)
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
    session_type_ = session_type;
}

void ClientUserAuthorizer::setUserName(const QString& username)
{
    username_ = username;
}

void ClientUserAuthorizer::setPassword(const QString& password)
{
    password_ = password;
}

void ClientUserAuthorizer::start()
{
    if (state_ != NotStarted)
    {
        qWarning("Authorizer already started");
        return;
    }

    proto::auth::ClientToHost message;

    // We do not support other authorization methods yet.
    message.mutable_logon_request()->set_method(proto::auth::METHOD_BASIC);

    state_ = Started;
    emit writeMessage(LogonRequest, serializeMessage(message));
}

void ClientUserAuthorizer::cancel()
{
    if (state_ == Finished)
        return;

    state_ = Finished;
    emit finished(proto::auth::STATUS_CANCELED);
}

void ClientUserAuthorizer::messageWritten(int message_id)
{
    if (state_ == Finished)
        return;

    emit readMessage();
}

void ClientUserAuthorizer::messageReceived(const QByteArray& buffer)
{
    if (state_ == Finished)
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

    emit errorOccurred(tr("Protocol error: Unknown message message from host."));
    cancel();
}

void ClientUserAuthorizer::readServerChallenge(
    const proto::auth::ServerChallenge& server_challenge)
{
    QByteArray nonce = QByteArray(server_challenge.nonce().c_str(),
                                  server_challenge.nonce().size());
    if (nonce.isEmpty())
    {
        emit errorOccurred(tr("Authorization error: Empty nonce is not allowed."));
        cancel();
        return;
    }

    if (username_.isEmpty() || password_.isEmpty())
    {
        AuthorizationDialog dialog;

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

    QByteArray session_key = createSessionKey(createPasswordHash(password_), nonce);

    proto::auth::ClientToHost message;

    proto::auth::ClientChallenge* client_challenge = message.mutable_client_challenge();
    client_challenge->set_session_type(session_type_);
    client_challenge->set_username(username_.toStdString());
    client_challenge->set_session_key(session_key.constData(), session_key.size());

    secureMemZero(&session_key);

    QByteArray serialized_message = serializeMessage(message);

    secureMemZero(client_challenge->mutable_username());
    secureMemZero(client_challenge->mutable_session_key());

    emit writeMessage(ClientChallenge, serialized_message);

    secureMemZero(&serialized_message);
}

void ClientUserAuthorizer::readLogonResult(const proto::auth::LogonResult& logon_result)
{
    state_ = Finished;
    emit finished(logon_result.status());
}

} // namespace aspia
