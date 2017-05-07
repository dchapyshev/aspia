//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_pool.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_pool.h"

namespace aspia {

ClientPool::ClientPool(std::shared_ptr<MessageLoopProxy> runner) :
    runner_(runner)
{
    DCHECK(runner_);
}

ClientPool::~ClientPool()
{
    network_client_.reset();
    session_list_.clear();
}

void ClientPool::Connect(HWND parent, const ClientConfig& config)
{
    config_.CopyFrom(config);

    status_dialog_.reset(new StatusDialog());
    status_dialog_->DoModal(parent, this);
    status_dialog_.reset();
}

void ClientPool::OnStatusDialogOpen()
{
    status_dialog_->SetDestonation(config_.RemoteAddress(), config_.RemotePort());
    status_dialog_->SetStatus(ClientStatus::CONNECTING);

    network_client_.reset(new NetworkClientTcp());

    if (!network_client_->Connect(config_.RemoteAddress(),
                                  config_.RemotePort(),
                                  this))
    {
        status_dialog_->SetStatus(ClientStatus::CONNECT_ERROR);
    }
}

void ClientPool::OnConnectionSuccess(std::unique_ptr<NetworkChannel> channel)
{
    status_dialog_->SetStatus(ClientStatus::CONNECTED);

    std::unique_ptr<Client> client(new Client(std::move(channel), config_, this));

    session_list_.push_back(std::move(client));

    // Leave from message loop.
    runner_->PostQuit();
}

void ClientPool::OnConnectionTimeout()
{
    status_dialog_->SetStatus(ClientStatus::CONNECT_TIMEOUT);
}

void ClientPool::OnConnectionError()
{
    status_dialog_->SetStatus(ClientStatus::CONNECT_ERROR);
}

void ClientPool::OnSessionTerminate()
{
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
