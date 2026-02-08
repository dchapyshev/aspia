//
// SmartCafe Project
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

namespace base {

//--------------------------------------------------------------------------------------------------
RelayPeerManager::RelayPeerManager(QObject* parent)
    : QObject(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
RelayPeerManager::~RelayPeerManager()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RelayPeerManager::addConnectionOffer(
    const proto::router::ConnectionOffer& offer, Authenticator* authenticator)
{
    RelayPeer* peer = new RelayPeer(authenticator, this);

    connect(peer, &RelayPeer::sig_connectionError, this, &RelayPeerManager::onRelayConnectionError);
    connect(peer, &RelayPeer::sig_connectionReady, this, &RelayPeerManager::onRelayConnectionReady);

    pending_.emplace_back(peer);
    peer->start(offer);
}

//--------------------------------------------------------------------------------------------------
QQueue<TcpChannel*> RelayPeerManager::takePendingConnections()
{
    for (const auto& channel : std::as_const(channels_))
        channel->setParent(nullptr);

    return std::move(channels_);
}

//--------------------------------------------------------------------------------------------------
void RelayPeerManager::onRelayConnectionReady()
{
    for (auto it = pending_.cbegin(), it_end = pending_.cend(); it != it_end; ++it)
    {
        RelayPeer* peer = *it;

        if (!peer->hasChannel())
            continue;

        TcpChannel* channel = peer->takeChannel();
        channel->setParent(this);

        channels_.emplace_back(channel);
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
        RelayPeer* peer = *it;

        if (peer->isFinished())
        {
            peer->deleteLater();
            it = pending_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace base
