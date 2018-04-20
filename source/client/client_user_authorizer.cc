//
// PROJECT:         Aspia
// FILE:            client/client_user_authorizer.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_user_authorizer.h"

#include <QCryptographicHash>
#include <QDebug>

#include "client/ui/authorization_dialog.h"
#include "base/message_serialization.h"

namespace aspia {

namespace {

enum MessageId { ResponseMessageId };

} // namespace

ClientUserAuthorizer::ClientUserAuthorizer(QWidget* parent)
    : QObject(parent)
{
    // Nothing
}

ClientUserAuthorizer::~ClientUserAuthorizer()
{
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

            nonce_ = QByteArray(request.nonce().c_str(), request.nonce().size());
            if (nonce_.isEmpty())
            {
                qWarning("Empty nonce not allowed");

                emit errorOccurred(tr("Protocol error: Invalid authorization request received."));
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

            QCryptographicHash password_hash(QCryptographicHash::Sha512);
            password_hash.addData(password_.toUtf8());

            QCryptographicHash key_hash(QCryptographicHash::Sha512);
            key_hash.addData(nonce_);
            key_hash.addData(password_hash.result());

            QByteArray key = key_hash.result();

            proto::auth::Response response;
            response.set_session_type(session_type_);
            response.set_username(username_.toStdString());
            response.set_key(key.constData(), key.size());

            state_ = ResponseWrite;
            emit writeMessage(ResponseMessageId, serializeMessage(response));
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
