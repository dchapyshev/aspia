//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host.h"
#include "host/host_session_desktop.h"
#include "host/host_session_power.h"
#include "host/host_user_list.h"
#include "proto/auth_session.pb.h"
#include "protocol/message_serialization.h"
#include "crypto/secure_string.h"

namespace aspia {

static const std::chrono::seconds kAuthTimeout{ 60 };

Host::Host(std::unique_ptr<NetworkChannel> channel, Delegate* delegate) :
    channel_(std::move(channel)),
    delegate_(delegate)
{
    channel_proxy_ = channel_->network_channel_proxy();
    channel_->StartListening(this);
}

Host::~Host()
{
    channel_.reset();
}

bool Host::IsAliveSession() const
{
    return channel_proxy_->IsConnected();
}

void Host::OnSessionMessage(const IOBuffer& buffer)
{
    channel_proxy_->Send(buffer);
}

void Host::OnSessionTerminate()
{
    channel_proxy_->Disconnect();
}

void Host::OnNetworkChannelConnect()
{
    // If the authorization request is not received within the specified time
    // interval, the connection will be closed.
    auth_timer_.Start(kAuthTimeout,
                      std::bind(&NetworkChannelProxy::Disconnect,
                                channel_proxy_.get()));
}

static proto::AuthStatus DoBasicAuthorization(const std::string& username,
                                              const std::string& password,
                                              proto::SessionType session_type)
{
    HostUserList list;

    if (!list.LoadFromStorage())
        return proto::AuthStatus::AUTH_STATUS_BAD_USERNAME_OR_PASSWORD;

    const int size = list.size();

    for (int i = 0; i < size; ++i)
    {
        const proto::HostUser& user = list.host_user(i);

        if (user.username() != username)
            continue;

        if (!user.enabled())
            return proto::AuthStatus::AUTH_STATUS_BAD_USERNAME_OR_PASSWORD;

        SecureString<std::string> password_hash;

        if (!HostUserList::CreatePasswordHash(password, password_hash))
            return proto::AuthStatus::AUTH_STATUS_BAD_USERNAME_OR_PASSWORD;

        if (user.password_hash() != password_hash)
            return proto::AuthStatus::AUTH_STATUS_BAD_USERNAME_OR_PASSWORD;

        if (!(user.session_types() & session_type))
            return proto::AuthStatus::AUTH_STATUS_SESSION_TYPE_NOT_ALLOWED;

        return proto::AuthStatus::AUTH_STATUS_SUCCESS;
    }

    return proto::AuthStatus::AUTH_STATUS_BAD_USERNAME_OR_PASSWORD;
}

bool Host::OnNetworkChannelFirstMessage(const SecureIOBuffer& buffer)
{
    // Authorization request received, stop the timer.
    auth_timer_.Stop();

    proto::auth::ClientToHost request;

    if (!ParseMessage(buffer, request))
        return false;

    proto::auth::HostToClient result;

    switch (request.method())
    {
        case proto::AuthMethod::AUTH_METHOD_BASIC:
            result.set_status(DoBasicAuthorization(request.username(),
                                                   request.password(),
                                                   request.session_type()));
            break;

        default:
            result.set_status(proto::AuthStatus::AUTH_STATUS_NOT_SUPPORTED_METHOD);
            break;
    }

    ClearStringContent(*request.mutable_username());
    ClearStringContent(*request.mutable_password());

    SecureIOBuffer result_buffer(SerializeSecureMessage(result));
    channel_proxy_->Send(result_buffer);

    if (result.status() == proto::AuthStatus::AUTH_STATUS_SUCCESS)
    {
        switch (request.session_type())
        {
            case proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE:
            case proto::SessionType::SESSION_TYPE_DESKTOP_VIEW:
                session_ = HostSessionDesktop::Create(request.session_type(), this);
                break;

            case proto::SessionType::SESSION_TYPE_POWER_MANAGE:
                session_ = HostSessionPower::Create(this);
                break;

            default:
                LOG(ERROR) << "Unsupported session type: " << request.session_type();
                break;
        }

        if (session_)
        {
            session_proxy_ = session_->host_session_proxy();
            return true;
        }
    }

    return false;
}

void Host::OnNetworkChannelMessage(const IOBuffer& buffer)
{
    DCHECK(session_proxy_);
    session_proxy_->Send(buffer);
}

void Host::OnNetworkChannelDisconnect()
{
    auth_timer_.Stop();
    session_.reset();
    delegate_->OnSessionTerminate();
}

} // namespace aspia
