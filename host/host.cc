//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host.h"
#include "host/host_session_desktop.h"
#include "host/host_session_power.h"
#include "host/host_user_utils.h"
#include "proto/auth_session.pb.h"
#include "protocol/message_serialization.h"
#include "crypto/sha512.h"

namespace aspia {

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

void Host::OnNetworkChannelMessage(const IOBuffer& buffer)
{
    std::unique_lock<std::mutex> lock(session_lock_);

    if (!is_auth_complete_)
    {
        is_auth_complete_ = SendAuthResult(buffer);
        if (!is_auth_complete_)
            channel_proxy_->Disconnect();

        return;
    }

    if (!session_)
        return;

    session_->Send(buffer);
}

void Host::OnNetworkChannelDisconnect()
{
    std::unique_lock<std::mutex> lock(session_lock_);
    session_.reset();

    delegate_->OnSessionTerminate();
}

static proto::AuthStatus BasicAuthorization(const std::string& username,
                                            const std::string& password,
                                            proto::SessionType session_type)
{
    proto::HostUserList list;

    if (ReadUserList(list))
    {
        for (int i = 0; i < list.user_list_size(); ++i)
        {
            const proto::HostUser& user = list.user_list(i);

            if (user.username() == username)
            {
                if (user.enabled())
                {
                    std::string password_hash;

                    if (CreateSHA512(password, password_hash))
                    {
                        if (user.password_hash() == password_hash)
                        {
                            if ((user.session_types() & session_type))
                            {
                                return proto::AuthStatus::AUTH_STATUS_SUCCESS;
                            }

                            return proto::AuthStatus::AUTH_STATUS_SESSION_TYPE_NOT_ALLOWED;
                        }
                    }
                }

                return proto::AuthStatus::AUTH_STATUS_BAD_USERNAME_OR_PASSWORD;
            }
        }
    }

    return proto::AuthStatus::AUTH_STATUS_BAD_USERNAME_OR_PASSWORD;
}

bool Host::SendAuthResult(const IOBuffer& request_buffer)
{
    proto::auth::ClientToHost request;

    if (!ParseMessage(request_buffer, request))
        return false;

    proto::auth::HostToClient result;

    switch (request.method())
    {
        case proto::AuthMethod::AUTH_METHOD_BASIC:
            result.set_status(BasicAuthorization(request.username(),
                                                 request.password(),
                                                 request.session_type()));
            break;

        default:
            result.set_status(proto::AuthStatus::AUTH_STATUS_NOT_SUPPORTED_METHOD);
            break;
    }

    IOBuffer result_buffer(SerializeMessage(result));
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

        return session_ != nullptr;
    }

    return false;
}

} // namespace aspia
