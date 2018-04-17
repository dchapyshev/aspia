//
// PROJECT:         Aspia
// FILE:            host/host_user_authorizer.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_user_authorizer.h"

#include <QCryptographicHash>
#include <QTimerEvent>

#include <random>

#include "base/message_serialization.h"
#include "network/channel.h"

namespace aspia {

namespace {

enum MessageId
{
    RequestMessageId,
    ResultMessageId
};

QByteArray generateNonce()
{
    static const size_t kNonceSize = 512; // 512 bytes.

    QByteArray nonce;
    nonce.resize(kNonceSize);

    std::random_device device;
    std::mt19937 enigne(device());
    std::uniform_int_distribution<quint64> dist(0, std::numeric_limits<quint64>::max());

    for (size_t i = 0; i < kNonceSize / sizeof(quint64); ++i)
    {
        quint64 random_number = dist(enigne);
        memcpy(nonce.data() + (i * sizeof(quint64)), &random_number, sizeof(quint64));
    }

    return nonce;
}

} // namespace

HostUserAuthorizer::HostUserAuthorizer(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

HostUserAuthorizer::~HostUserAuthorizer() = default;

void HostUserAuthorizer::setUserList(const UserList& user_list)
{
    user_list_ = user_list;
}

void HostUserAuthorizer::setChannel(Channel* channel)
{
    channel_.reset(channel);
}

void HostUserAuthorizer::start()
{
    emit started();

    if (user_list_.isEmpty() || channel_.isNull())
    {
        abort();
        return;
    }

    // If authorization is not completed within the specified time interval
    // the connection will be closed.
    timer_id_ = startTimer(std::chrono::minutes(2));

    connect(channel_.data(), &Channel::channelDisconnected, this, &HostUserAuthorizer::abort);
    connect(channel_.data(), &Channel::channelMessage, this, &HostUserAuthorizer::readMessage);
    connect(channel_.data(), &Channel::messageWritten, this, &HostUserAuthorizer::messageWritten);

    nonce_ = generateNonce();

    proto::auth::Request request;

    request.set_version(0);
    request.set_nonce(nonce_.constData(), nonce_.size());

    channel_->writeMessage(RequestMessageId, serializeMessage(request));
}

void HostUserAuthorizer::abort()
{
    if (timer_id_)
    {
        killTimer(timer_id_);
        timer_id_ = 0;
    }

    channel_.reset();
    emit finished();
}

void HostUserAuthorizer::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != timer_id_)
        return;

    abort();
}

void HostUserAuthorizer::messageWritten(int message_id)
{
    switch (message_id)
    {
        case RequestMessageId:
        {
            // Authorization request sent. We are reading the response.
            channel_->readMessage();
        }
        break;

        case ResultMessageId:
        {
            if (status_ == proto::auth::STATUS_SUCCESS)
                emit createSession(session_type_, channel_.take());

            emit finished();
        }
        break;

        default:
        {
            qFatal("Unexpected message id: %d", message_id);
            abort();
        }
        break;
    }
}

void HostUserAuthorizer::readMessage(const QByteArray& buffer)
{
    killTimer(timer_id_);
    timer_id_ = 0;

    proto::auth::Response response;
    if (!parseMessage(buffer, response))
    {
        abort();
        return;
    }

    QString username = QString::fromStdString(response.username());
    QByteArray key = QByteArray(response.key().c_str(), response.key().size());

    proto::auth::Result result;

    for (const auto& user : user_list_)
    {
        if (user.name().compare(username, Qt::CaseInsensitive) != 0)
            continue;

        QCryptographicHash key_hash(QCryptographicHash::Sha512);
        key_hash.addData(nonce_);
        key_hash.addData(user.passwordHash());

        if (key_hash.result() != key)
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

    channel_->writeMessage(ResultMessageId, serializeMessage(result));
}

} // namespace aspia
