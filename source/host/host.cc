//
// PROJECT:         Aspia
// FILE:            host/host.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host.h"

#include <QDebug>
#include <QCryptographicHash>
#include <QString>

#include "crypto/random.h"
#include "host/user_list.h"
#include "proto/auth_session.pb.h"
#include "protocol/message_serialization.h"

namespace aspia {

namespace {

constexpr std::chrono::seconds kAuthTimeout{ 60 };

proto::auth::Status DoAuthorization(const QString& username,
                                    const QByteArray& key,
                                    const QByteArray& nonce,
                                    proto::auth::SessionType session_type)
{
    UserList user_list = ReadUserList();

    for (const auto& user : user_list)
    {
        if (user.name().compare(username, Qt::CaseInsensitive) != 0)
            continue;

        QCryptographicHash key_hash(QCryptographicHash::Sha512);
        key_hash.addData(nonce);
        key_hash.addData(user.passwordHash());

        if (key_hash.result() != key)
            return proto::auth::STATUS_ACCESS_DENIED;

        if (!(user.flags() & User::FLAG_ENABLED))
            return proto::auth::STATUS_ACCESS_DENIED;

        if (!(user.sessions() & session_type))
            return proto::auth::STATUS_ACCESS_DENIED;

        return proto::auth::STATUS_SUCCESS;
    }

    return proto::auth::STATUS_ACCESS_DENIED;
}

} // namespace

Host::Host(std::shared_ptr<NetworkChannel> channel, Delegate* delegate)
    : channel_(std::move(channel)),
      delegate_(delegate)
{
    channel_proxy_ = channel_->network_channel_proxy();

    channel_->StartChannel(std::bind(
        &Host::OnNetworkChannelStatusChange, this, std::placeholders::_1));
}

Host::~Host()
{
    channel_.reset();
}

bool Host::IsTerminatedSession() const
{
    return channel_proxy_->IsDiconnecting();
}

void Host::OnNetworkChannelStatusChange(NetworkChannel::Status status)
{
    if (status == NetworkChannel::Status::CONNECTED)
    {
        // If authorization is not completed within the specified time interval
        // the connection will be closed.
        auth_timer_.Start(kAuthTimeout,
                          std::bind(&NetworkChannelProxy::Disconnect, channel_proxy_));

        nonce_ = CreateRandomBuffer(512);

        proto::auth::Request request;
        request.set_nonce(nonce_.data(), nonce_.size());

        channel_proxy_->Send(SerializeMessage(request), std::bind(&Host::OnRequestSended, this));
    }
    else
    {
        Q_ASSERT(status == NetworkChannel::Status::DISCONNECTED);

        auth_timer_.Stop();
        session_.reset();

        delegate_->OnSessionTerminate();
    }
}

void Host::OnRequestSended()
{
    channel_proxy_->Receive(std::bind(&Host::OnResponseReceived, this, std::placeholders::_1));
}

void Host::OnResponseReceived(const QByteArray& buffer)
{
    proto::auth::Response response;

    if (!ParseMessage(buffer, response))
    {
        channel_proxy_->Disconnect();
        return;
    }

    proto::auth::Status status =
        DoAuthorization(QString::fromUtf8(response.username().c_str(), response.username().size()),
                        QByteArray(response.key().c_str(), response.key().size()),
                        nonce_,
                        response.session_type());

    proto::auth::Result result;
    result.set_status(status);

    channel_proxy_->Send(SerializeMessage(result),
                         std::bind(&Host::OnResultSended, this, response.session_type(), status));
}

void Host::OnResultSended(proto::auth::SessionType session_type, proto::auth::Status status)
{
    // Authorization result sended, stop the timer.
    auth_timer_.Stop();

    if (status != proto::auth::STATUS_SUCCESS)
    {
        channel_proxy_->Disconnect();
        return;
    }

    switch (session_type)
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
        case proto::auth::SESSION_TYPE_FILE_TRANSFER:
        // case proto::auth::SESSION_TYPE_SYSTEM_INFO:
        {
            session_ = std::make_unique<HostSession>(session_type, channel_proxy_);
        }
        break;

        default:
        {
            qWarning() << "Unsupported session type: " << session_type;
            channel_proxy_->Disconnect();
        }
        break;
    }
}

} // namespace aspia
