//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client.h"
#include "client/client_session_desktop_manage.h"
#include "client/client_session_power_manage.h"
#include "protocol/message_serialization.h"
#include "ui/auth_dialog.h"

namespace aspia {

Client::Client(std::unique_ptr<NetworkChannel> channel,
               const ClientConfig& config,
               Delegate* delegate) :
    channel_(std::move(channel)),
    config_(config),
    delegate_(delegate)
{
    channel_proxy_ = channel_->network_channel_proxy();
    channel_->StartListening(this);
}

Client::~Client()
{
    channel_.reset();
}

bool Client::IsAliveSession() const
{
    return channel_proxy_->IsConnected();
}

void Client::OnSessionMessageAsync(IOBuffer buffer)
{
    channel_proxy_->SendAsync(std::move(buffer));
}

void Client::OnSessionMessage(const IOBuffer& buffer)
{
    channel_proxy_->Send(buffer);
}

void Client::OnSessionTerminate()
{
    channel_proxy_->Disconnect();
}

void Client::OnNetworkChannelMessage(const IOBuffer& buffer)
{
    std::unique_lock<std::mutex> lock(session_lock_);

    if (!is_auth_complete_)
    {
        is_auth_complete_ = ReadAuthResult(buffer);
        if (!is_auth_complete_)
        {
            channel_proxy_->Disconnect();
        }
        else
        {
            CreateSession(config_.SessionType());
        }

        return;
    }

    if (!session_)
        return;

    session_->Send(buffer);
}

void Client::OnNetworkChannelDisconnect()
{
    std::unique_lock<std::mutex> lock(session_lock_);
    session_.reset();

    delegate_->OnSessionTerminate();
}

void Client::OnNetworkChannelStarted()
{
    std::unique_lock<std::mutex> lock(session_lock_);

    AuthDialog dialog;
    if (dialog.DoModal(nullptr) != IDOK)
    {
        channel_proxy_->Disconnect();
        return;
    }

    proto::auth::ClientToHost request;

    request.set_method(proto::AuthMethod::AUTH_METHOD_BASIC);
    request.set_session_type(config_.SessionType());

    request.set_username(dialog.UserName());
    request.set_password(dialog.Password());

    IOBuffer output_buffer = SerializeMessage(request);
    CHECK(!output_buffer.IsEmpty());

    channel_proxy_->Send(output_buffer);
}

void Client::OnStatusDialogOpen()
{
    status_dialog_->SetDestonation(config_.RemoteAddress(),
                                   config_.RemotePort());
    status_dialog_->SetStatus(status_);
}

bool Client::ReadAuthResult(const IOBuffer& buffer)
{
    proto::auth::HostToClient result;
    
    if (!ParseMessage(buffer, result))
        return false;

    switch (result.status())
    {
        case proto::AuthStatus::AUTH_STATUS_SUCCESS:
            return true;

        case proto::AuthStatus::AUTH_STATUS_BAD_USERNAME_OR_PASSWORD:
            status_ = ClientStatus::INVALID_USERNAME_OR_PASSWORD;
            break;

        case proto::AuthStatus::AUTH_STATUS_NOT_SUPPORTED_METHOD:
            status_ = ClientStatus::NOT_SUPPORTED_AUTH_METHOD;
            break;

        case proto::AuthStatus::AUTH_STATUS_SESSION_TYPE_NOT_ALLOWED:
            status_ = ClientStatus::SESSION_TYPE_NOT_ALLOWED;
            break;

        default:
            LOG(ERROR) << "Unknown auth status: " << result.status();
            return false;
    }

    status_dialog_.reset(new StatusDialog());
    status_dialog_->DoModal(nullptr, this);
    status_dialog_.reset();

    return false;
}

void Client::CreateSession(proto::SessionType session_type)
{
    switch (session_type)
    {
        case proto::SessionType::SESSION_DESKTOP_MANAGE:
            session_.reset(new ClientSessionDesktopManage(config_, this));
            break;

        case proto::SessionType::SESSION_POWER_MANAGE:
            session_.reset(new ClientSessionPowerManage(config_, this));
            break;

        default:
            LOG(FATAL) << "Invalid session type: " << session_type;
            break;
    }
}

} // namespace aspia
