//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client.h"
#include "client/client_session_desktop_manage.h"
#include "client/client_session_file_transfer.h"
#include "client/client_session_power_manage.h"
#include "crypto/secure_string.h"
#include "protocol/message_serialization.h"
#include "ui/auth_dialog.h"

namespace aspia {

Client::Client(std::shared_ptr<NetworkChannel> channel,
               const ClientConfig& config,
               Delegate* delegate)
    : channel_(std::move(channel)),
      config_(config),
      delegate_(delegate),
      status_dialog_(this)
{
    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

Client::~Client()
{
    ui_thread_.Stop();
}

void Client::OnBeforeThreadRunning()
{
    runner_ = ui_thread_.message_loop_proxy();
    channel_proxy_ = channel_->network_channel_proxy();
    channel_->StartChannel(std::bind(
        &Client::OnNetworkChannelStatusChange, this, std::placeholders::_1));
}

void Client::OnAfterThreadRunning()
{
    channel_.reset();
}

bool Client::IsAliveSession() const
{
    return channel_ != nullptr;
}

void Client::OnSessionMessage(IOBuffer buffer)
{
    channel_proxy_->Send(std::move(buffer), nullptr);
}

void Client::OnSessionTerminate()
{
    channel_.reset();
}

void Client::OnNetworkChannelMessage(IOBuffer buffer)
{
    session_proxy_->Send(std::move(buffer));

    channel_proxy_->Receive(
        std::bind(&Client::OnNetworkChannelMessage, this, std::placeholders::_1));
}

void Client::OnNetworkChannelStatusChange(NetworkChannel::Status status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(
            std::bind(&Client::OnNetworkChannelStatusChange, this, status));
        return;
    }

    if (status == NetworkChannel::Status::CONNECTED)
    {
        UiAuthDialog auth_dialog;

        if (auth_dialog.DoModal(nullptr) != IDOK)
        {
            channel_.reset();
            return;
        }

        proto::auth::ClientToHost request;

        request.set_method(proto::AuthMethod::AUTH_METHOD_BASIC);
        request.set_session_type(config_.session_type());

        request.set_username(auth_dialog.UserName());
        request.set_password(auth_dialog.Password());

        channel_proxy_->Send(SerializeMessage<IOBuffer>(request),
                             std::bind(&Client::OnAuthRequestSended, this));

        SecureClearString(*request.mutable_username());
        SecureClearString(*request.mutable_password());
    }
    else
    {
        DCHECK(status == NetworkChannel::Status::DISCONNECTED);

        session_.reset();
        ui_thread_.StopSoon();
        delegate_->OnSessionTerminate();
    }
}

void Client::OnStatusDialogOpen()
{
    status_dialog_.SetDestonation(config_.address(), config_.port());
    status_dialog_.SetStatus(status_);
}

void Client::OpenStatusDialog()
{
    status_dialog_.DoModal(nullptr);
}

void Client::OnAuthRequestSended()
{
    channel_proxy_->Receive(
        std::bind(&Client::DoAuthorize, this, std::placeholders::_1));
}

void Client::DoAuthorize(IOBuffer buffer)
{
    proto::auth::HostToClient result;

    if (ParseMessage(buffer, result))
    {
        status_ = result.status();

        if (status_ == proto::Status::STATUS_SUCCESS)
        {
            CreateSession(result.session_type());

            channel_proxy_->Receive(
                std::bind(&Client::OnNetworkChannelMessage, this, std::placeholders::_1));

            return;
        }

        runner_->PostTask(std::bind(&Client::OpenStatusDialog, this));
    }

    channel_.reset();
}

void Client::CreateSession(proto::SessionType session_type)
{
    switch (session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
            session_.reset(new ClientSessionDesktopManage(config_, this));
            break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
            session_.reset(new ClientSessionDesktopView(config_, this));
            break;

        case proto::SESSION_TYPE_FILE_TRANSFER:
            session_.reset(new ClientSessionFileTransfer(config_, this));
            break;

        case proto::SESSION_TYPE_POWER_MANAGE:
            session_.reset(new ClientSessionPowerManage(config_, this));
            break;

        default:
            LOG(FATAL) << "Invalid session type: " << session_type;
            return;
    }

    session_proxy_ = session_->client_session_proxy();
}

} // namespace aspia
