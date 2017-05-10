//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_pool.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_pool.h"

namespace aspia {

HostPool::~HostPool()
{
    terminating_ = true;

    if (runner_)
    {
        DCHECK(runner_->BelongsToCurrentThread());
    }

    network_server_.reset();
    session_list_.clear();
}

bool HostPool::Start()
{
    runner_ = MessageLoopProxy::Current();
    DCHECK(runner_);

    network_server_.reset(new NetworkServerTcp());

    return network_server_->Start(kDefaultHostTcpPort, this);
}

void HostPool::OnChannelConnected(std::unique_ptr<NetworkChannel> channel)
{
    std::unique_ptr<Host> host(new Host(std::move(channel), this));
    session_list_.push_back(std::move(host));
}

void HostPool::OnSessionTerminate()
{
    if (terminating_)
        return;

    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&HostPool::OnSessionTerminate, this));
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
