//
// PROJECT:         Aspia
// FILE:            host/host_pool.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_pool.h"

namespace aspia {

HostPool::HostPool(std::shared_ptr<MessageLoopProxy> runner)
    : runner_(runner)
{
    Q_ASSERT(runner_);

    network_server_ = std::make_unique<NetworkServerTcp>(
        kDefaultHostTcpPort,
        std::bind(&HostPool::OnChannelConnected, this, std::placeholders::_1));
}

HostPool::~HostPool()
{
    Q_ASSERT(runner_->BelongsToCurrentThread());

    terminating_ = true;

    network_server_.reset();
    session_list_.clear();
}

void HostPool::OnChannelConnected(std::shared_ptr<NetworkChannel> channel)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&HostPool::OnChannelConnected, this, channel));
        return;
    }

    session_list_.emplace_back(new Host(channel, this));
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
