//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_local_pool.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "client/client_local_system_info_pool.h"

namespace aspia {

ClientLocalSystemInfoPool::ClientLocalSystemInfoPool(std::shared_ptr<MessageLoopProxy> runner)
    : runner_(runner)
{
    DCHECK(runner_);
}

ClientLocalSystemInfoPool::~ClientLocalSystemInfoPool()
{
    terminating_ = true;
    list_.clear();
}

void ClientLocalSystemInfoPool::Open()
{
    DCHECK(runner_->BelongsToCurrentThread());
    list_.emplace_back(new ClientLocalSystemInfo(this));
}

void ClientLocalSystemInfoPool::OnClose()
{
    if (terminating_)
        return;

    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&ClientLocalSystemInfoPool::OnClose, this));
        return;
    }

    auto iter = list_.begin();

    while (iter != list_.end())
    {
        if (iter->get()->IsTerminated())
        {
            iter = list_.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
}

} // namespace aspia
