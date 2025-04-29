//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/net/tcp_channel.h"

namespace base {

//--------------------------------------------------------------------------------------------------
RelayPeerManager::RelayPeerManager(QObject* parent)
    : QObject(parent)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
RelayPeerManager::~RelayPeerManager()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RelayPeerManager::addConnectionOffer(const proto::ConnectionOffer& offer)
{
    std::unique_ptr<RelayPeer> peer = std::make_unique<RelayPeer>();

    connect(peer.get(), &RelayPeer::sig_connectionError,
            this, &RelayPeerManager::onRelayConnectionError);
    connect(peer.get(), &RelayPeer::sig_connectionReady,
            this, &RelayPeerManager::onRelayConnectionReady);

    pending_.emplace_back(std::move(peer));
    pending_.back()->start(offer);
}

//--------------------------------------------------------------------------------------------------
bool RelayPeerManager::hasPendingConnections() const
{
    return !channels_.empty();
}

//--------------------------------------------------------------------------------------------------
TcpChannel* RelayPeerManager::nextPendingConnection()
{
    if (channels_.empty())
        return nullptr;

    TcpChannel* channel = channels_.front().release();
    channels_.pop();
    return channel;
}

//--------------------------------------------------------------------------------------------------
void RelayPeerManager::onRelayConnectionReady()
{
    for (auto it = pending_.cbegin(), it_end = pending_.cend(); it != it_end; ++it)
    {
        if (!it->get()->hasChannel())
            continue;

        channels_.emplace(std::unique_ptr<TcpChannel>(it->get()->takeChannel()));
        emit sig_newPeerConnected();
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
            it->release()->deleteLater();
            it = pending_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace base
