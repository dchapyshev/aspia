//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/peer/relay_peer_manager.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "base/net/tcp_channel.h"

namespace base {

//--------------------------------------------------------------------------------------------------
RelayPeerManager::RelayPeerManager(std::shared_ptr<TaskRunner> task_runner, Delegate* delegate)
    : task_runner_(std::move(task_runner)),
      delegate_(delegate)
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(task_runner_ && delegate_);
}

//--------------------------------------------------------------------------------------------------
RelayPeerManager::~RelayPeerManager()
{
    LOG(LS_INFO) << "Dtor";
    delegate_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void RelayPeerManager::addConnectionOffer(const proto::ConnectionOffer& offer)
{
    pending_.emplace_back(std::make_unique<RelayPeer>());
    pending_.back()->start(offer, this);
}

//--------------------------------------------------------------------------------------------------
void RelayPeerManager::onRelayConnectionReady(std::unique_ptr<TcpChannel> channel)
{
    if (delegate_)
    {
        delegate_->onNewPeerConnected(std::move(channel));
    }
    else
    {
        LOG(LS_ERROR) << "Invalid delegate";
    }

    cleanup();
}

//--------------------------------------------------------------------------------------------------
void RelayPeerManager::onRelayConnectionError()
{
    cleanup();
}

//--------------------------------------------------------------------------------------------------
void RelayPeerManager::cleanup()
{
    auto it = pending_.begin();
    while (it != pending_.end())
    {
        if (it->get()->isFinished())
        {
            task_runner_->deleteSoon(std::move(*it));
            it = pending_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace base
