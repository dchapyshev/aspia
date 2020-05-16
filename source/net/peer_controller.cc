//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "net/peer_controller.h"

#include "base/logging.h"

namespace net {

PeerController::PeerController()
{
    // TODO
}

PeerController::~PeerController()
{
    // TODO
}

void PeerController::start(const RouterList& router_list, Delegate* delegate)
{
    router_list_ = router_list;
    delegate_ = delegate;

    if (router_list_.empty() || !delegate_)
    {
        LOG(LS_ERROR) << "Invalid parameters";
        return;
    }

    current_router_ = 0;
    connectToRouter();
}

void PeerController::connectTo(uint64_t peer_id)
{
    // TODO
}

void PeerController::onConnected()
{
    // TODO
}

void PeerController::onDisconnected(Channel::ErrorCode error_code)
{
    // TODO
}

void PeerController::onMessageReceived(const base::ByteArray& buffer)
{
    // TODO
}

void PeerController::onMessageWritten(size_t /* pending */)
{
    // TODO
}

void PeerController::connectToRouter()
{
    const Router& current = router_list_[current_router_];

    channel_ = std::make_unique<Channel>();
    channel_->connect(current.address, current.port);
}

} // namespace net
