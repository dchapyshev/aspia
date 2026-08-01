//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/peer/authenticator.h"
#include "base/peer/relay_peer.h"
#include "base/threading/worker.h"

namespace {

// Each offer from the router starts an outgoing connection to the relay. Legitimate peers connect
// one at a time, so the limit only cuts off a flood of offers.
const int kMaxPendingPeers = 16;

// Resolving the address of the relay and the TCP handshake. Same as the default for
// TcpChannel::connectTo, which watches over the very same phase.
const Seconds kConnectTimeout{ 30 };

} // namespace

//--------------------------------------------------------------------------------------------------
RelayPeerManager::RelayPeerManager(QObject* parent)
    : QObject(parent)
{
    LOG(INFO) << "Ctor";

    connect(Worker::current(), &Worker::sig_tick, this, &RelayPeerManager::onTimer);
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
    if (pending_.size() >= kMaxPendingPeers)
    {
        LOG(ERROR) << "Too many pending relay connections:" << pending_.size();

        // Ownership was passed to us, but there is no peer to hand it over to.
        authenticator->deleteLater();
        return;
    }

    RelayPeer* peer = new RelayPeer(authenticator, this);

    connect(peer, &RelayPeer::sig_connectionError, this, &RelayPeerManager::onRelayConnectionError);
    connect(peer, &RelayPeer::sig_connectionReady, this, &RelayPeerManager::onRelayConnectionReady);

    pending_.emplace_back(peer, Clock::now());
    peer->start(offer);
}

//--------------------------------------------------------------------------------------------------
bool RelayPeerManager::hasPendingConnections() const
{
    return !ready_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
RelayPeerManager::ReadyConnection RelayPeerManager::takePendingConnection()
{
    if (ready_.isEmpty())
        return ReadyConnection(nullptr, proto::router::ConnectionOffer());

    ReadyConnection ready = ready_.takeFirst();
    ready.first->setParent(nullptr);

    return ready;
}

//--------------------------------------------------------------------------------------------------
void RelayPeerManager::onRelayConnectionReady()
{
    // Only the peer that has just finished authentication gives up its channel. hasChannel() is
    // true from the moment the channel is created, so walking the whole pending list here used to
    // pick up the channels of peers that are still authenticating.
    RelayPeer* peer = qobject_cast<RelayPeer*>(sender());
    if (peer && peer->hasChannel())
    {
        TcpChannel* channel = peer->takeChannel();
        channel->setParent(this);

        ready_.emplace_back(channel, peer->connectionOffer());
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
void RelayPeerManager::onTimer(TimePoint now)
{
    // A peer has no watchdog of its own, so an offer from the router leaves an outgoing connection
    // hanging until the OS gives up on the SYN retransmits. Once the channel exists the peer is
    // authenticating, and that phase is limited by the timeout inside Authenticator.
    auto it = pending_.begin();
    while (it != pending_.end())
    {
        if (it->peer->hasChannel() || now - it->start_time < kConnectTimeout)
        {
            ++it;
            continue;
        }

        LOG(WARNING) << "Dropped relay connection stuck in the connect phase";

        it->peer->disconnect(this);
        it->peer->deleteLater();
        it = pending_.erase(it);
    }
}

//--------------------------------------------------------------------------------------------------
void RelayPeerManager::cleanup()
{
    auto it = pending_.begin();
    while (it != pending_.end())
    {
        RelayPeer* peer = it->peer;

        if (peer->isFinished())
        {
            peer->disconnect(this);
            peer->deleteLater();
            it = pending_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
