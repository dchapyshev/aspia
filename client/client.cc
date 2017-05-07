//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client.h"
#include "client/client_session_desktop.h"
#include "protocol/message_serialization.h"
#include "ui/auth_dialog.h"

namespace aspia {

Client::Client(std::unique_ptr<NetworkChannel> channel,
               const ClientConfig& config,
               Delegate* delegate) :
    channel_(std::move(channel)),
    config_(config),
    is_auth_complete_(false),
    delegate_(delegate)
{
    channel_->StartListening(this);
}

Client::~Client()
{
    channel_.reset();
}

bool Client::IsAliveSession() const
{
    return channel_->IsConnected();
}

void Client::OnSessionMessage(std::unique_ptr<IOBuffer> buffer)
{
    channel_->SendAsync(std::move(buffer));
}

void Client::OnSessionTerminate()
{
    channel_->Close();
}

void Client::OnNetworkChannelMessage(const IOBuffer* buffer)
{
    std::unique_lock<std::mutex> lock(session_lock_);

    if (!is_auth_complete_)
    {
        is_auth_complete_ = ReadAuthResult(buffer);
        if (!is_auth_complete_)
        {
            channel_->Close();
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
        channel_->Close();
        return;
    }

    proto::auth::ClientToHost request;

    request.set_method(proto::AuthMethod::AUTH_METHOD_BASIC);
    request.set_session_type(config_.SessionType());

    request.set_username(dialog.UserName());
    request.set_password(dialog.Password());

    std::unique_ptr<IOBuffer> output_buffer = SerializeMessage(request);
    CHECK(output_buffer);

    channel_->Send(output_buffer.get());
}

void Client::OnStatusDialogOpen()
{
    status_dialog_->SetDestonation(config_.RemoteAddress(),
                                   config_.RemotePort());
    status_dialog_->SetStatus(status_);
}

bool Client::ReadAuthResult(const IOBuffer* buffer)
{
    std::unique_ptr<proto::auth::HostToClient> result =
        ParseMessage<proto::auth::HostToClient>(buffer);

    if (!result)
        return false;

    switch (result->status())
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
            LOG(ERROR) << "Unknown auth status: " << result->status();
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
            session_.reset(new ClientSessionDesktop(config_, this));
            break;

        default:
            LOG(FATAL) << "Invalid session type: " << session_type;
            break;
    }
}

} // namespace aspia
