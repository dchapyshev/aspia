//
// PROJECT:         Aspia
// FILE:            client/client_pool.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_pool.h"

namespace aspia {

ClientPool::ClientPool(std::shared_ptr<MessageLoopProxy> runner)
    : runner_(runner),
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

void ClientPool::Connect(HWND parent, const proto::Computer& computer)
{
    computer_.CopyFrom(computer);
    status_dialog_.DoModal(parent);
    network_client_.reset();
}

void ClientPool::OnStatusDialogOpen()
{
    if (!NetworkClientTcp::IsValidHostName(computer_.address()))
    {
        status_dialog_.SetConnectionStatus(StatusDialog::ConnectionStatus::INVALID_ADDRESS);
    }
    else if (!NetworkClientTcp::IsValidPort(computer_.port()))
    {
        status_dialog_.SetConnectionStatus(StatusDialog::ConnectionStatus::INVALID_PORT);
    }
    else
    {
        status_dialog_.SetDestonation(computer_.address(), computer_.port());
        status_dialog_.SetConnectionStatus(StatusDialog::ConnectionStatus::CONNECTING);

        network_client_ = std::make_unique<NetworkClientTcp>(
            computer_.address(),
            computer_.port(),
            std::bind(&ClientPool::OnConnect, this, std::placeholders::_1));
    }
}

void ClientPool::OnConnect(std::shared_ptr<NetworkChannel> channel)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&ClientPool::OnConnect, this, channel));
        return;
    }

    if (!channel)
    {
        status_dialog_.SetConnectionStatus(StatusDialog::ConnectionStatus::CONNECT_ERROR);
        return;
    }

    session_list_.emplace_back(new Client(channel, computer_, this));

    status_dialog_.EndDialog(0);
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
        if (iter->get()->IsTerminatedSession())
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
