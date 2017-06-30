//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_pool.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_pool.h"

namespace aspia {

ClientPool::ClientPool(std::shared_ptr<MessageLoopProxy> runner) :
    runner_(runner),
    status_dialog_(this)
{
    DCHECK(runner_);
}

ClientPool::~ClientPool()
{
    terminating_ = true;
    network_client_.reset();
    session_list_.clear();
}

void ClientPool::Connect(HWND parent, const ClientConfig& config)
{
    config_.CopyFrom(config);
    status_dialog_.DoModal(parent);
}

void ClientPool::OnStatusDialogOpen()
{
    if (!NetworkClientTcp::IsValidHostName(config_.address()))
    {
        status_dialog_.SetStatus(proto::Status::STATUS_INVALID_ADDRESS);
    }
    else if (!NetworkClientTcp::IsValidPort(config_.port()))
    {
        status_dialog_.SetStatus(proto::Status::STATUS_INVALID_PORT);
    }
    else
    {
        status_dialog_.SetDestonation(config_.address(), config_.port());
        status_dialog_.SetStatus(proto::Status::STATUS_CONNECTING);

        network_client_.reset(new NetworkClientTcp(runner_));

        if (!network_client_->Connect(config_.address(), config_.port(), this))
        {
            status_dialog_.SetStatus(proto::Status::STATUS_CONNECT_ERROR);
        }
    }
}

void ClientPool::OnConnectionSuccess(std::unique_ptr<NetworkChannel> channel)
{
    std::unique_ptr<Client> client(new Client(std::move(channel), config_, this));
    session_list_.push_back(std::move(client));

    status_dialog_.EndDialog(0);
}

void ClientPool::OnConnectionTimeout()
{
    status_dialog_.SetStatus(proto::Status::STATUS_CONNECT_TIMEOUT);
}

void ClientPool::OnConnectionError()
{
    status_dialog_.SetStatus(proto::Status::STATUS_CONNECT_ERROR);
}

void ClientPool::OnSessionTerminate()
{
    if (terminating_)
        return;

    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&ClientPool::OnSessionTerminate, this));
        return;
    }

    auto iter = session_list_.begin();

    while (iter != session_list_.end())
    {
        if (!iter->get()->IsAliveSession())
        {
            iter = session_list_.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
}

} // namespace aspia
