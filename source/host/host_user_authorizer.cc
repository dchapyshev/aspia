//
// PROJECT:         Aspia
// FILE:            host/host_user_authorizer.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
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

// TODO: Configuring the number of iterations in the UI.
const quint32 kKeyHashingRounds = 15000;

enum MessageId
{
    RequestMessageId,
    ResultMessageId
};

QByteArray generateNonce()
{
    // TODO: Configure the size in the UI.
    static const size_t kNonceSize = 512; // 512 bytes.
    return Random::generateBuffer(kNonceSize);
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
}

NetworkChannel* HostUserAuthorizer::networkChannel()
{
    return network_channel_;
}

proto::auth::Status HostUserAuthorizer::status() const
{
    return status_;
}

proto::auth::SessionType HostUserAuthorizer::sessionType() const
{
    return session_type_;
}

QString HostUserAuthorizer::userName() const
{
    return user_name_;
}

void HostUserAuthorizer::start()
{
    if (state_ != NotStarted)
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

    connect(network_channel_, &NetworkChannel::disconnected,
            this, &HostUserAuthorizer::stop);

    connect(network_channel_, &NetworkChannel::messageReceived,
            this, &HostUserAuthorizer::messageReceived);

    connect(network_channel_, &NetworkChannel::messageWritten,
            this, &HostUserAuthorizer::messageWritten);

    connect(this, &HostUserAuthorizer::writeMessage,
            network_channel_, &NetworkChannel::writeMessage);

    connect(this, &HostUserAuthorizer::readMessage,
            network_channel_, &NetworkChannel::readMessage);

    nonce_ = generateNonce();
    if (nonce_.isEmpty())
    {
        qDebug("Empty nonce generated");
        stop();
        return;
    }

    proto::auth::Request request;

    request.set_hashing(proto::auth::HASHING_SHA512);
    request.set_rounds(kKeyHashingRounds);
    request.set_nonce(nonce_.constData(), nonce_.size());

    QByteArray serialized_message = serializeMessage(request);

    secureMemZero(request.mutable_nonce());

    state_ = RequestWrite;
    emit writeMessage(RequestMessageId, serialized_message);

    secureMemZero(&serialized_message);
}

void HostUserAuthorizer::stop()
{
    if (state_ == Finished)
        return;

    if (timer_id_)
    {
        killTimer(timer_id_);
        timer_id_ = 0;
    }

    connect(network_channel_, &NetworkChannel::disconnected,
            network_channel_, &NetworkChannel::deleteLater);

    network_channel_->stop();
    network_channel_ = nullptr;

    state_ = Finished;
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

void HostUserAuthorizer::messageWritten(int message_id)
{
    if (state_ == Finished)
        return;

    switch (message_id)
    {
        case RequestMessageId:
        {
            Q_ASSERT(state_ == RequestWrite);

            // Authorization request sent. We are reading the response.
            state_ = WaitForResponse;
            emit readMessage();
        }
        break;

        case ResultMessageId:
        {
            Q_ASSERT(state_ == ResultWrite);

            if (status_ != proto::auth::STATUS_SUCCESS)
            {
                connect(network_channel_, &NetworkChannel::disconnected,
                        network_channel_, &NetworkChannel::deleteLater);

                network_channel_->stop();
                network_channel_ = nullptr;
            }

            state_ = Finished;
            emit finished(this);
        }
        break;

        default:
        {
            qFatal("Unexpected message id: %d", message_id);
            stop();
        }
        break;
    }
}

void HostUserAuthorizer::messageReceived(const QByteArray& buffer)
{
    if (state_ == Finished)
        return;

    Q_ASSERT(state_ == WaitForResponse);
    Q_ASSERT(timer_id_);

    killTimer(timer_id_);
    timer_id_ = 0;

    proto::auth::Response response;
    if (!parseMessage(buffer, response))
    {
        stop();
        return;
    }

    QByteArray key(response.key().c_str(), response.key().size());
    user_name_ = QString::fromStdString(response.username());

    if (user_name_.isEmpty() || key.isEmpty())
    {
        stop();
        return;
    }

    status_ = proto::auth::STATUS_ACCESS_DENIED;

    for (const auto& user : user_list_)
    {
        if (user.name().compare(user_name_, Qt::CaseInsensitive) != 0)
            continue;

        if (createKey(user.passwordHash(), nonce_, kKeyHashingRounds) != key)
        {
            status_ = proto::auth::STATUS_ACCESS_DENIED;
            break;
        }

        if (!(user.flags() & User::FLAG_ENABLED))
        {
            status_ = proto::auth::STATUS_ACCESS_DENIED;
            break;
        }

        if (!(user.sessions() & response.session_type()))
        {
            status_ = proto::auth::STATUS_ACCESS_DENIED;
            break;
        }

        status_ = proto::auth::STATUS_SUCCESS;
        break;
    }

    secureMemZero(response.mutable_username());
    secureMemZero(response.mutable_key());
    secureMemZero(&key);

    session_type_ = response.session_type();
    state_ = ResultWrite;

    proto::auth::Result result;
    result.set_status(status_);

    emit writeMessage(ResultMessageId, serializeMessage(result));
}

} // namespace aspia
