//
// PROJECT:         Aspia
// FILE:            host/host.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host.h"

#include "base/strings/unicode.h"
#include "host/host_user_list.h"
#include "proto/auth_session.pb.h"
#include "protocol/authorization.h"
#include "protocol/message_serialization.h"

namespace aspia {

namespace {

constexpr std::chrono::seconds kAuthTimeout{ 60 };
constexpr uint32_t kAllowedMethods = proto::auth::METHOD_BASIC;

proto::auth::Status DoBasicAuthorization(proto::auth::SessionType session_type,
                                         const std::string& key,
                                         const std::string& nonce)
{
    HostUserList user_list;

    if (!user_list.LoadFromStorage())
        return proto::auth::STATUS_ACCESS_DENIED;

    for (int i = 0; i < user_list.size(); ++i)
    {
        const proto::HostUser& user = user_list.host_user(i);

        if (CreateUserKey(UNICODEfromUTF8(user.username()), user.password_hash(), nonce) != key)
            continue;

        if (!user.enabled())
            return proto::auth::STATUS_ACCESS_DENIED;

        if (!(user.session_types() & session_type))
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

        proto::auth::AllowMethods allow_methods;

        allow_methods.set_methods(kAllowedMethods);

        channel_proxy_->Send(SerializeMessage(allow_methods),
                             std::bind(&Host::OnAllowMethodsSended, this));
    }
    else
    {
        DCHECK(status == NetworkChannel::Status::DISCONNECTED);

        auth_timer_.Stop();
        session_.reset();

        delegate_->OnSessionTerminate();
    }
}

void Host::OnAllowMethodsSended()
{
    channel_proxy_->Receive(std::bind(&Host::OnSelectMethodReceived, this, std::placeholders::_1));
}

void Host::OnSelectMethodReceived(const IOBuffer& buffer)
{
    proto::auth::SelectMethod select_method;

    if (!ParseMessage(buffer, select_method))
    {
        channel_proxy_->Disconnect();
        return;
    }

    if (!(select_method.method() & kAllowedMethods))
    {
        channel_proxy_->Disconnect();
        return;
    }

    nonce_ = CreateNonce();

    proto::auth::BasicRequest request;
    request.set_nonce(nonce_);

    channel_proxy_->Send(SerializeMessage(request), std::bind(&Host::OnRequestSended, this));
}

void Host::OnRequestSended()
{
    channel_proxy_->Receive(std::bind(&Host::OnResponseReceived, this, std::placeholders::_1));
}

void Host::OnResponseReceived(const IOBuffer& buffer)
{
    proto::auth::BasicResponse response;

    if (!ParseMessage(buffer, response))
    {
        channel_proxy_->Disconnect();
        return;
    }

    proto::auth::Status status =
        DoBasicAuthorization(response.session_type(), response.key(), nonce_);

    proto::auth::BasicResult result;
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
        case proto::auth::SESSION_TYPE_SYSTEM_INFO:
        {
            session_ = std::make_unique<HostSession>(session_type, channel_proxy_);
        }
        break;

        default:
        {
            LOG(LS_ERROR) << "Unsupported session type: " << session_type;
            channel_proxy_->Disconnect();
        }
        break;
    }
}

} // namespace aspia
