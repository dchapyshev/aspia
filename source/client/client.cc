//
// PROJECT:         Aspia
// FILE:            client/client.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client.h"
#include "crypto/secure_memory.h"
#include "crypto/sha.h"
#include "protocol/message_serialization.h"
#include "ui/auth_dialog.h"

namespace aspia {

namespace {

constexpr size_t kNonceSize = 512;

std::string CreateUserKey(const std::string& username,
                          const std::string& password_hash,
                          const std::string& nonce)
{
    DCHECK_EQ(nonce.size(), kNonceSize);

    StreamSHA512 sha512;

    sha512.AppendData(nonce);
    sha512.AppendData(username);
    sha512.AppendData(password_hash);

    return sha512.Final();
}

} // namespace

Client::Client(std::shared_ptr<NetworkChannel> channel,
               const proto::Computer& computer,
               Delegate* delegate)
    : channel_(std::move(channel)),
      computer_(computer),
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

bool Client::IsTerminatedSession() const
{
    return channel_proxy_->IsDiconnecting();
}

void Client::OnNetworkChannelStatusChange(NetworkChannel::Status status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&Client::OnNetworkChannelStatusChange, this, status));
        return;
    }

    if (status == NetworkChannel::Status::CONNECTED)
    {
        channel_proxy_->Receive(std::bind(&Client::OnRequestReceived, this, std::placeholders::_1));
    }
    else
    {
        DCHECK(status == NetworkChannel::Status::DISCONNECTED);

        session_.reset();
        ui_thread_.StopSoon();
        delegate_->OnSessionTerminate();
    }
}

void Client::OnRequestReceived(const IOBuffer& buffer)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&Client::OnRequestReceived, this, buffer));
        return;
    }

    proto::auth::Request request;

    if (!ParseMessage(buffer, request))
    {
        channel_proxy_->Disconnect();
        return;
    }

    if (request.method() != proto::auth::METHOD_BASIC)
    {
        channel_proxy_->Disconnect();
        return;
    }

    AuthDialog auth_dialog;

    if (auth_dialog.DoModal(nullptr) != IDOK)
    {
        channel_proxy_->Disconnect();
        return;
    }

    proto::auth::Response response;
    response.set_session_type(computer_.session_type());
    response.set_key(CreateUserKey(auth_dialog.UserName(),
                                   SHA512(auth_dialog.Password(), 1000),
                                   request.nonce()));

    channel_proxy_->Send(SerializeMessage(response), std::bind(&Client::OnResponseSended, this));
}

void Client::OnResponseSended()
{
    channel_proxy_->Receive(std::bind(&Client::OnResultReceived, this, std::placeholders::_1));
}

void Client::OnResultReceived(const IOBuffer& buffer)
{
    proto::auth::Result result;

    if (ParseMessage(buffer, result))
    {
        status_ = result.status();

        if (status_ == proto::auth::STATUS_SUCCESS)
        {
            session_ = ClientSession::Create(computer_, channel_proxy_);
            if (session_)
                return;
        }
        else
        {
            runner_->PostTask(std::bind(&Client::OpenStatusDialog, this));
        }
    }

    channel_proxy_->Disconnect();
}

void Client::OnStatusDialogOpen()
{
    status_dialog_.SetDestonation(computer_.address(), computer_.port());
    status_dialog_.SetAuthorizationStatus(status_);
}

void Client::OpenStatusDialog()
{
    status_dialog_.DoModal(nullptr);
}

} // namespace aspia
