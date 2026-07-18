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

#include "client/workers/router_worker.h"

#include <optional>

#include "base/logging.h"
#include "base/net/address.h"
#include "base/net/tcp_channel_ng.h"
#include "base/peer/client_authenticator.h"
#include "build/build_config.h"
#include "client/config.h"
#include "client/database.h"
#include "proto/key_exchange.h"

namespace {

// Number of one-second timer ticks to wait before retrying a dropped connection.
const int kReconnectTicks = 3;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterWorker::RouterWorker()
    : Worker(Thread::AsioDispatcher, Seconds(1))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
RouterWorker::~RouterWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onConnect(qint64 router_id)
{
    if (connections_.contains(router_id))
        return;

    connections_.insert(router_id, Connection());
    startConnection(router_id);
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onDisconnect(qint64 router_id)
{
    auto it = connections_.find(router_id);
    if (it == connections_.end())
        return;

    if (it->channel)
    {
        it->channel->disconnect();
        it->channel->deleteLater();
    }

    connections_.erase(it);
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onSendMessage(qint64 router_id, quint8 channel_id, const QByteArray& buffer)
{
    auto it = connections_.find(router_id);
    if (it == connections_.end() || !it->channel)
    {
        LOG(WARNING) << "Dropping outgoing message, channel not ready for router" << router_id;
        return;
    }

    it->channel->send(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onStart()
{
    LOG(INFO) << "Router worker started";
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onStop()
{
    LOG(INFO) << "Router worker stopped";

    for (const Connection& connection : std::as_const(connections_))
    {
        if (connection.channel)
        {
            connection.channel->disconnect();
            connection.channel->deleteLater();
        }
    }

    connections_.clear();
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onTimer()
{
    // One-second tick: retry every dropped connection whose countdown has elapsed.
    for (auto it = connections_.begin(); it != connections_.end(); ++it)
    {
        if (it->channel || it->reconnect_countdown == 0)
            continue;

        if (--it->reconnect_countdown == 0)
            startConnection(it.key());
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::startConnection(qint64 router_id)
{
    auto it = connections_.find(router_id);
    if (it == connections_.end() || it->channel)
        return;

    // The worker owns the connection, so it reads the credentials from the database itself (thread-
    // local connection) instead of shuttling them across threads on every connect.
    const std::optional<RouterConfig> config = Database::instance().findRouter(router_id);
    if (!config)
    {
        LOG(ERROR) << "No config for router" << router_id;
        it->reconnect_countdown = kReconnectTicks;
        return;
    }

    LOG(INFO) << "Connecting to router" << router_id << config->address();

    const Address addr = Address::fromString(config->address(), DEFAULT_ROUTER_CLIENT_TCP_PORT);

    auto* authenticator = new ClientAuthenticator();
    authenticator->setIdentify(proto::key_exchange::IDENTIFY_SRP);
    authenticator->setSessionType(static_cast<quint32>(config->sessionType()));
    authenticator->setUserName(config->username());
    authenticator->setPassword(config->password());

    // The channel takes ownership of the authenticator and lives on this worker thread.
    TcpChannel* channel = new TcpChannelNG(authenticator, this);
    it->channel = channel;

    connect(channel, &TcpChannel::sig_authenticated, this, [this, router_id]()
    {
        onChannelAuthenticated(router_id);
    });
    connect(channel, &TcpChannel::sig_errorOccurred, this, [this, router_id](TcpChannel::ErrorCode error_code)
    {
        onChannelErrorOccurred(router_id, error_code);
    });
    connect(channel, &TcpChannel::sig_messageReceived, this,
            [this, router_id](quint8 channel_id, const QByteArray& buffer)
    {
        emit sig_messageReceived(router_id, channel_id, buffer);
    });

    channel->connectTo(addr.host(), addr.port());
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onChannelAuthenticated(qint64 router_id)
{
    auto it = connections_.find(router_id);
    if (it == connections_.end() || !it->channel)
        return;

    // Authentication passed; let the router replies flow and hand the peer version to Router.
    it->channel->setPaused(false);
    emit sig_authenticated(router_id, it->channel->peerVersion());
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onChannelErrorOccurred(qint64 router_id, TcpChannel::ErrorCode error_code)
{
    auto it = connections_.find(router_id);
    if (it == connections_.end())
        return;

    if (it->channel)
    {
        it->channel->disconnect();
        it->channel->deleteLater();
        it->channel = nullptr;
    }

    // The one-second timer retries once the countdown elapses.
    it->reconnect_countdown = kReconnectTicks;

    emit sig_errorOccurred(router_id, error_code);
}
