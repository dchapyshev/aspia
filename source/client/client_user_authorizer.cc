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

enum MessageId { ResponseMessageId };

QByteArray createPasswordHash(const QString& password)
{
    static const int kIterCount = 100000;

    QByteArray data = password.toUtf8();

    for (int i = 0; i < kIterCount; ++i)
    {
        data = QCryptographicHash::hash(data, QCryptographicHash::Sha512);
    }

    return data;
}

QByteArray createKey(const QByteArray& password_hash, const QByteArray& nonce, quint32 rounds)
{
    QByteArray data = password_hash;

    for (quint32 i = 0; i < rounds; ++i)
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

proto::auth::SessionType ClientUserAuthorizer::sessionType() const
{
    return session_type_;
}

void ClientUserAuthorizer::setSessionType(proto::auth::SessionType session_type)
{
    session_type_ = session_type;
}

QString ClientUserAuthorizer::userName() const
{
    return username_;
}

void ClientUserAuthorizer::setUserName(const QString& username)
{
    username_ = username;
}

QString ClientUserAuthorizer::password() const
{
    return password_;
}

void ClientUserAuthorizer::setPassword(const QString& password)
{
    password_ = password;
}

void ClientUserAuthorizer::start()
{
    state_ = WaitForRequest;
    emit readMessage();
}

void ClientUserAuthorizer::cancel()
{
    if (state_ == NotStarted || state_ == Finished)
        return;

    state_ = Finished;
    emit finished(proto::auth::STATUS_CANCELED);
}

void ClientUserAuthorizer::messageWritten(int message_id)
{
    switch (state_)
    {
        case ResponseWrite:
        {
            Q_ASSERT(message_id == ResponseMessageId);

            state_ = WaitForResult;
            emit readMessage();
        }
        break;

        default:
            qFatal("Unexpected state: %d", state_);
            break;
    }
}

void ClientUserAuthorizer::messageReceived(const QByteArray& buffer)
{
    switch (state_)
    {
        case WaitForRequest:
        {
            proto::auth::Request request;
            if (!parseMessage(buffer, request))
            {
                emit errorOccurred(tr("Protocol error: Invalid authorization request received."));
                cancel();
                return;
            }

            if (request.hashing() != proto::auth::HASHING_SHA512)
            {
                emit errorOccurred(tr("Authorization error: Unsupported hashing algorithm."));
                cancel();
                return;
            }

            if (!request.rounds())
            {
                emit errorOccurred(tr("Authorization error: Invalid number of hashing rounds."));
                cancel();
                return;
            }

            nonce_ = QByteArray(request.nonce().c_str(), request.nonce().size());
            if (nonce_.isEmpty())
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

            QByteArray key = createKey(createPasswordHash(password_), nonce_, request.rounds());

            proto::auth::Response response;
            response.set_session_type(session_type_);
            response.set_username(username_.toStdString());
            response.set_key(key.constData(), key.size());

            secureMemZero(&key);

            state_ = ResponseWrite;

            QByteArray serialized_message = serializeMessage(response);

            secureMemZero(response.mutable_username());
            secureMemZero(response.mutable_key());

            emit writeMessage(ResponseMessageId, serialized_message);

            secureMemZero(&serialized_message);
        }
        break;

        case WaitForResult:
        {
            proto::auth::Result result;
            if (!parseMessage(buffer, result))
            {
                emit errorOccurred(tr("Protocol error: Invalid authorization result received."));
                cancel();
                return;
            }

            state_ = Finished;
            emit finished(result.status());
        }
        break;

        default:
            qFatal("Unexpected state: %d", state_);
            break;
    }
}

} // namespace aspia
