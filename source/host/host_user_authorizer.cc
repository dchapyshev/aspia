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
#include "network/network_channel.h"

namespace aspia {

namespace {

const quint32 kKeyHashingRounds = 15000;

enum MessageId
{
    RequestMessageId,
    ResultMessageId
};

QByteArray generateNonce()
{
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
}

void HostUserAuthorizer::setUserList(const UserList& user_list)
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
        qWarning("Empty nonce");
        stop();
        return;
    }

    proto::auth::Request request;

    request.set_version(proto::auth::VERSION_1_0_0);
    request.set_hashing(proto::auth::HASHING_SHA512);
    request.set_rounds(kKeyHashingRounds);
    request.set_nonce(nonce_.constData(), nonce_.size());

    state_ = RequestWrite;
    emit writeMessage(RequestMessageId, serializeMessage(request));
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

    const std::string& key = response.key();

    if (response.username().empty() || key.empty())
    {
        stop();
        return;
    }

    QString username = QString::fromStdString(response.username());

    proto::auth::Result result;

    for (const auto& user : user_list_)
    {
        if (user.name().compare(username, Qt::CaseInsensitive) != 0)
            continue;

        if (createKey(user.passwordHash(), nonce_, kKeyHashingRounds) !=
                QByteArray(key.c_str(), key.size()))
        {
            result.set_status(proto::auth::STATUS_ACCESS_DENIED);
            break;
        }

        if (!(user.flags() & User::FLAG_ENABLED))
        {
            result.set_status(proto::auth::STATUS_ACCESS_DENIED);
            break;
        }

        if (!(user.sessions() & response.session_type()))
        {
            result.set_status(proto::auth::STATUS_ACCESS_DENIED);
            break;
        }

        result.set_status(proto::auth::STATUS_SUCCESS);
        break;
    }

    session_type_ = response.session_type();
    status_ = result.status();

    state_ = ResultWrite;
    emit writeMessage(ResultMessageId, serializeMessage(result));
}

} // namespace aspia
