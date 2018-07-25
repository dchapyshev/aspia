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

#include "host/host_user_authorizer.h"

#include <QCryptographicHash>
#include <QTimerEvent>

#include "base/message_serialization.h"
#include "crypto/random.h"
#include "crypto/secure_memory.h"
#include "network/network_channel.h"

namespace aspia {

namespace {

const int kMaxAttemptCount = 3;
const int kKeyHashingRounds = 100000;
const int kNonceSize = 16;

QByteArray generateNonce()
{
    return Random::generateBuffer(kNonceSize);
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

HostUserAuthorizer::HostUserAuthorizer(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

HostUserAuthorizer::~HostUserAuthorizer()
{
    stop();

    secureMemZero(&user_name_);
    secureMemZero(&nonce_);
}

void HostUserAuthorizer::setUserList(const QList<User>& user_list)
{
    user_list_ = user_list;
}

void HostUserAuthorizer::setNetworkChannel(NetworkChannel* network_channel)
{
    network_channel_ = network_channel;
    network_channel_->setParent(this);
}

void HostUserAuthorizer::start()
{
    if (state_ != State::NOT_STARTED)
    {
        qWarning("Authorizer already started");
        return;
    }

    if (user_list_.isEmpty() || network_channel_.isNull())
    {
        qWarning("Empty user list or invalid network channel");
        stop();
        return;
    }

    // If authorization is not completed within the specified time interval
    // the connection will be closed.
    timer_id_ = startTimer(std::chrono::minutes(2));
    if (!timer_id_)
    {
        qWarning("Unable to start timer");
        stop();
        return;
    }

    connect(network_channel_, &NetworkChannel::disconnected,
            this, &HostUserAuthorizer::stop);

    connect(network_channel_, &NetworkChannel::messageReceived,
            this, &HostUserAuthorizer::messageReceived);

    state_ = State::STARTED;
    network_channel_->start();
}

void HostUserAuthorizer::stop()
{
    if (state_ == State::FINISHED)
        return;

    state_ = State::FINISHED;

    if (timer_id_)
    {
        killTimer(timer_id_);
        timer_id_ = 0;
    }

    if (network_channel_)
    {
        network_channel_->stop();
        network_channel_ = nullptr;
    }

    session_type_ = proto::auth::SESSION_TYPE_UNKNOWN;
    status_ = proto::auth::STATUS_CANCELED;

    emit finished(this);
}

void HostUserAuthorizer::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != timer_id_)
        return;

    stop();
}

void HostUserAuthorizer::messageReceived(const QByteArray& buffer)
{
    if (state_ == State::FINISHED)
        return;

    proto::auth::ClientToHost message;
    if (parseMessage(buffer, message))
    {
        if (message.has_logon_request())
        {
            readLogonRequest(message.logon_request());
            return;
        }
        else if (message.has_client_challenge())
        {
            readClientChallenge(message.client_challenge());

            secureMemZero(message.mutable_client_challenge()->mutable_username());
            secureMemZero(message.mutable_client_challenge()->mutable_session_key());
            return;
        }
    }

    qWarning("Unknown message from client");
    stop();
}

void HostUserAuthorizer::readLogonRequest(const proto::auth::LogonRequest& logon_request)
{
    // We do not support other authorization methods yet.
    if (logon_request.method() != proto::auth::METHOD_BASIC)
    {
        qWarning() << "Unsupported authorization method: " << logon_request.method();
        status_ = proto::auth::STATUS_ACCESS_DENIED;
        writeLogonResult(status_);
        return;
    }

    method_ = logon_request.method();

    nonce_ = generateNonce();
    if (nonce_.isEmpty())
    {
        qDebug("Empty nonce generated");
        stop();
        return;
    }

    writeServerChallenge(nonce_);
}

void HostUserAuthorizer::readClientChallenge(const proto::auth::ClientChallenge& client_challenge)
{
    if (nonce_.isEmpty())
    {
        qWarning("Unexpected client challenge. Nonce not generated yet");
        stop();
        return;
    }

    QByteArray session_key = QByteArray::fromStdString(client_challenge.session_key());
    user_name_ = QString::fromStdString(client_challenge.username());
    session_type_ = client_challenge.session_type();

    status_ = doBasicAuthorization(user_name_, session_key, session_type_);

    secureMemZero(&session_key);
    writeLogonResult(status_);
}

void HostUserAuthorizer::writeServerChallenge(const QByteArray& nonce)
{
    if (network_channel_)
    {
        proto::auth::HostToClient message;
        message.mutable_server_challenge()->set_nonce(nonce.toStdString());
        network_channel_->send(serializeMessage(message));
    }
}

void HostUserAuthorizer::writeLogonResult(proto::auth::Status status)
{
    ++attempt_count_;

    // If the number of authorization attempts is exceeded, then we disconnect the connection.
    // The authorization result is not sent.
    if (status != proto::auth::STATUS_SUCCESS && attempt_count_ >= kMaxAttemptCount)
    {
        qWarning("The number of authorization attempts has been exceeded.");
        stop();
        return;
    }

    if (network_channel_)
    {
        proto::auth::HostToClient message;
        message.mutable_logon_result()->set_status(status);
        network_channel_->send(serializeMessage(message));
    }

    // Authorization passed or exceeded the number of attempts. Finish.
    if (status == proto::auth::STATUS_SUCCESS)
    {
        Q_ASSERT(timer_id_ != 0);

        state_ = State::FINISHED;

        if (network_channel_)
            network_channel_->pause();

        killTimer(timer_id_);
        timer_id_ = 0;

        emit finished(this);
    }
}

proto::auth::Status HostUserAuthorizer::doBasicAuthorization(
    const QString& user_name, const QByteArray& session_key, proto::auth::SessionType session_type)
{
    if (!User::isValidName(user_name))
    {
        qWarning() << "Invalid user name: " << user_name;
        return proto::auth::STATUS_ACCESS_DENIED;
    }

    if (session_key.isEmpty())
    {
        qWarning("Empty session key");
        return proto::auth::STATUS_ACCESS_DENIED;
    }

    for (const auto& user : user_list_)
    {
        if (user.name().compare(user_name, Qt::CaseInsensitive) == 0)
        {
            if (createSessionKey(user.passwordHash(), nonce_) != session_key)
            {
                qWarning() << "Wrong password for user" << user_name;
                return proto::auth::STATUS_ACCESS_DENIED;
            }

            if (!(user.flags() & User::FLAG_ENABLED))
            {
                qWarning() << "User" << user_name << "is disabled";
                return proto::auth::STATUS_ACCESS_DENIED;
            }

            if (!(user.sessions() & session_type))
            {
                qWarning() << "Session type " << session_type
                           << " is disabled for user " << user_name;
                return proto::auth::STATUS_ACCESS_DENIED;
            }

            return proto::auth::STATUS_SUCCESS;
        }
    }

    qWarning() << "User not found: " << user_name;
    return proto::auth::STATUS_ACCESS_DENIED;
}

} // namespace aspia
