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

#include "peer/peer_controller.h"

#include "base/logging.h"

namespace peer {

PeerController::PeerController()
{
    // TODO
}

PeerController::~PeerController()
{
    // TODO
}

void PeerController::start(const RouterInfo& router_info, Delegate* delegate)
{
    router_info_ = router_info;
    delegate_ = delegate;

    if (!delegate_)
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

void PeerController::onDisconnected(base::NetworkChannel::ErrorCode error_code)
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
    channel_ = std::make_unique<base::NetworkChannel>();
    channel_->connect(router_info_.address, router_info_.port);
}

} // namespace base
