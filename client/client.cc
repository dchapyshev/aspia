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

namespace aspia {

Client::Client(std::unique_ptr<NetworkChannel> channel,
               const ClientConfig& config,
               Delegate* delegate) :
    channel_(std::move(channel)),
    config_(config),
    delegate_(delegate)
{
    ui_thread_.Start(MessageLoop::Type::TYPE_UI, this);
}

Client::~Client()
{
    ui_thread_.Stop();
}

void Client::OnBeforeThreadRunning()
{
    runner_ = ui_thread_.message_loop_proxy();
    channel_proxy_ = channel_->network_channel_proxy();
    channel_->StartListening(this);
}

void Client::OnAfterThreadRunning()
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
    if (!session_proxy_)
    {
        if (ReadAuthResult(buffer))
        {
            CreateSession(config_.session_type());
        }
        else
        {
            channel_proxy_->Disconnect();
        }

        return;
    }

    session_proxy_->Send(buffer);
}

void Client::OnNetworkChannelDisconnect()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&Client::OnNetworkChannelDisconnect, this));
        return;
    }

    session_.reset();
    ui_thread_.StopSoon();
    delegate_->OnSessionTerminate();
}

void Client::OnNetworkChannelStarted()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&Client::OnNetworkChannelStarted, this));
        return;
    }

    AuthDialog auth_dialog;

    if (auth_dialog.DoModal(nullptr) != IDOK)
    {
        channel_proxy_->Disconnect();
        return;
    }

    proto::auth::ClientToHost request;

    request.set_method(proto::AuthMethod::AUTH_METHOD_BASIC);
    request.set_session_type(config_.session_type());

    request.set_username(auth_dialog.UserName());
    request.set_password(auth_dialog.Password());

    IOBuffer output_buffer(SerializeMessage(request));
    CHECK(!output_buffer.IsEmpty());

    channel_proxy_->Send(output_buffer);
}

void Client::OnStatusDialogOpen()
{
    status_dialog_.SetDestonation(config_.address(), config_.port());
    status_dialog_.SetStatus(status_);
}

void Client::OpenStatusDialog()
{
    status_dialog_.DoModal(nullptr, this);
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
            status_ = ClientStatus::UNKNOWN;
            break;
    }

    runner_->PostTask(std::bind(&Client::OpenStatusDialog, this));
    return false;
}

void Client::CreateSession(proto::SessionType session_type)
{
    switch (session_type)
    {
        case proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE:
            session_.reset(new ClientSessionDesktopManage(config_, this));
            break;

        case proto::SessionType::SESSION_TYPE_DESKTOP_VIEW:
            session_.reset(new ClientSessionDesktopView(config_, this));
            break;

        case proto::SessionType::SESSION_TYPE_POWER_MANAGE:
            session_.reset(new ClientSessionPowerManage(config_, this));
            break;

        default:
            LOG(FATAL) << "Invalid session type: " << session_type;
            return;
    }

    session_proxy_ = session_->client_session_proxy();
}

} // namespace aspia
