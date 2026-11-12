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

#include "base/build_config.h"
#include "base/logging.h"
#include "base/net/address.h"
#include "base/net/tcp_channel_proxy.h"
#include "base/peer/client_authenticator.h"
#include "client/config.h"
#include "client/storage/database.h"
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
void RouterWorker::onConnect()
{
    if (connection_)
        return;

    connection_.emplace();
    startConnection();
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onDisconnect()
{
    if (!connection_)
        return;

    if (connection_->channel)
    {
        connection_->channel->disconnect();
        connection_->channel->deleteLater();
    }

    connection_.reset();
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onReconnect()
{
    if (!connection_)
        connection_.emplace();

    if (connection_->channel)
    {
        connection_->channel->disconnect();
        connection_->channel->deleteLater();
        connection_->channel = nullptr;
    }

    connection_->reconnect_countdown = kReconnectTicks;
    emit sig_reconnecting();
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onSendMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (!connection_ || !connection_->channel)
    {
        LOG(WARNING) << "Dropping outgoing message, channel not ready";
        return;
    }

    connection_->channel->send(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onStart()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onStop()
{
    LOG(INFO) << "Router worker stopped";
    onDisconnect();
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onTimer(TimePoint now)
{
    if (!connection_)
        return;

    if (connection_->channel)
    {
        connection_->channel->tick(now);
        return;
    }

    if (connection_->reconnect_countdown != 0 && --connection_->reconnect_countdown == 0)
        startConnection();
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::startConnection()
{
    if (!connection_ || connection_->channel)
        return;

    Database& db = Database::instance();

    RouterConfig config;
    if (db.routerConfig(&config) != Database::FindResult::FOUND || !config.isValid() || !config.isEnabled())
    {
        LOG(ERROR) << "No usable config for router";
        connection_->reconnect_countdown = kReconnectTicks;
        return;
    }

    ProxyConfig proxy;
    if (db.proxyConfig(&proxy) != Database::ReadResult::OK)
    {
        LOG(ERROR) << "No readable proxy config for router";
        connection_->reconnect_countdown = kReconnectTicks;
        return;
    }

    LOG(INFO) << "Connecting to router" << config.address()
              << (proxy.settings().enabled ? "through proxy" : "directly");

    const Address addr = Address::fromString(config.address(), kDefaultRouterClientTcpPort);

    auto* authenticator = new ClientAuthenticator();
    authenticator->setIdentify(proto::key_exchange::IDENTIFY_SRP);
    authenticator->setSessionType(static_cast<quint32>(config.sessionType()));
    authenticator->setUserName(config.username());
    authenticator->setPassword(config.password());

    // The channel takes ownership of the authenticator and lives on this worker thread.
    TcpChannel* channel = TcpChannelProxy::create(
        TcpChannel::Type::DIRECT, proxy.settings(), authenticator, this);
    connection_->channel = channel;

    connect(channel, &TcpChannel::sig_authenticated, this, &RouterWorker::onChannelAuthenticated);
    connect(channel, &TcpChannel::sig_errorOccurred, this, &RouterWorker::onChannelErrorOccurred);
    connect(channel, &TcpChannel::sig_messageReceived, this, &RouterWorker::sig_messageReceived);

    channel->connectTo(addr.host(), addr.port());
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onChannelAuthenticated()
{
    if (!connection_ || !connection_->channel)
        return;

    // Authentication passed; let the router replies flow and hand the peer version to the
    // controller.
    connection_->channel->setPaused(false);
    emit sig_authenticated(connection_->channel->peerVersion());
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onChannelErrorOccurred(TcpChannel::ErrorCode error_code)
{
    if (!connection_)
        return;

    if (connection_->channel)
    {
        connection_->channel->disconnect();
        connection_->channel->deleteLater();
        connection_->channel = nullptr;
    }

    // The one-second timer retries once the countdown elapses.
    connection_->reconnect_countdown = kReconnectTicks;

    emit sig_errorOccurred(error_code);
}
