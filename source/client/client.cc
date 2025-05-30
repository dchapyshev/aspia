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

#include "client/client.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "client/status_window_proxy.h"

#if defined(OS_MAC)
#include "base/mac/app_nap_blocker.h"
#endif // defined(OS_MAC)

namespace client {

//--------------------------------------------------------------------------------------------------
Client::Client(std::shared_ptr<base::TaskRunner> io_task_runner)
    : io_task_runner_(std::move(io_task_runner))
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(io_task_runner_);

#if defined(OS_MAC)
    base::addAppNapBlock();
#endif // defined(OS_MAC)
}

//--------------------------------------------------------------------------------------------------
Client::~Client()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK(io_task_runner_->belongsToCurrentThread());
    stop();

#if defined(OS_MAC)
    base::releaseAppNapBlock();
#endif // defined(OS_MAC)
}

//--------------------------------------------------------------------------------------------------
void Client::start()
{
    DCHECK(io_task_runner_->belongsToCurrentThread());
    DCHECK(status_window_proxy_);

    if (state_ != State::CREATED)
    {
        LOG(LS_ERROR) << "Client already started before";
        return;
    }

    if (!session_state_)
    {
        LOG(LS_ERROR) << "Session state not installed";
        return;
    }

    state_ = State::STARTED;
    is_connected_to_router_ = false;

    Config config = session_state_->config();

    if (base::isHostId(config.address_or_id))
    {
        LOG(LS_INFO) << "Starting RELAY connection";

        if (!config.router_config.has_value())
        {
            LOG(LS_FATAL) << "No router config. Continuation is impossible";
            return;
        }

        router_controller_ =
            std::make_unique<RouterController>(*config.router_config, io_task_runner_);

        bool reconnecting = session_state_->isReconnecting();
        if (!reconnecting)
        {
            // Show the status window.
            status_window_proxy_->onStarted();
        }

        status_window_proxy_->onRouterConnecting();
        router_controller_->connectTo(base::stringToHostId(config.address_or_id), reconnecting, this);
    }
    else
    {
        LOG(LS_INFO) << "Starting DIRECT connection";

        if (!session_state_->isReconnecting())
        {
            // Show the status window.
            status_window_proxy_->onStarted();
        }

        // Create a network channel for messaging.
        channel_ = std::make_unique<base::TcpChannel>();

        // Set the listener for the network channel.
        channel_->setListener(this);

        // Now connect to the host.
        status_window_proxy_->onHostConnecting();
        channel_->connect(config.address_or_id, config.port);
    }
}

//--------------------------------------------------------------------------------------------------
void Client::stop()
{
    DCHECK(io_task_runner_->belongsToCurrentThread());

    if (state_ != State::STOPPPED)
    {
        LOG(LS_INFO) << "Stopping client...";
        state_ = State::STOPPPED;

        router_controller_.reset();
        authenticator_.reset();
        channel_.reset();
        timeout_timer_.reset();

        session_state_->setAutoReconnect(false);
        session_state_->setReconnecting(false);

        status_window_proxy_->onStopped();

        LOG(LS_INFO) << "Client stopped";
    }
    else
    {
        LOG(LS_ERROR) << "Client already stopped";
    }
}

//--------------------------------------------------------------------------------------------------
void Client::setStatusWindow(std::shared_ptr<StatusWindowProxy> status_window_proxy)
{
    LOG(LS_INFO) << "Status window installed";
    status_window_proxy_ = std::move(status_window_proxy);
}

//--------------------------------------------------------------------------------------------------
void Client::setSessionState(std::shared_ptr<SessionState> session_state)
{
    LOG(LS_INFO) << "Session state installed";
    session_state_ = session_state;
}

//--------------------------------------------------------------------------------------------------
void Client::sendMessage(uint8_t channel_id, const google::protobuf::MessageLite& message)
{
    if (!channel_)
    {
        LOG(LS_ERROR) << "sendMessage called but channel not initialized";
        return;
    }

    channel_->send(channel_id, serializer_.serialize(message));
}

//--------------------------------------------------------------------------------------------------
int64_t Client::totalRx() const
{
    if (!channel_)
    {
        LOG(LS_ERROR) << "totalRx called but channel not initialized";
        return 0;
    }

    return channel_->totalRx();
}

//--------------------------------------------------------------------------------------------------
int64_t Client::totalTx() const
{
    if (!channel_)
    {
        LOG(LS_ERROR) << "totalTx called but channel not initialized";
        return 0;
    }

    return channel_->totalTx();
}

//--------------------------------------------------------------------------------------------------
int Client::speedRx()
{
    if (!channel_)
    {
        LOG(LS_ERROR) << "speedRx called but channel not initialized";
        return 0;
    }

    return channel_->speedRx();
}

//--------------------------------------------------------------------------------------------------
int Client::speedTx()
{
    if (!channel_)
    {
        LOG(LS_ERROR) << "speedTx called but channel not initialized";
        return 0;
    }

    return channel_->speedTx();
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpConnected()
{
    LOG(LS_INFO) << "Connection established";
    startAuthentication();
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    LOG(LS_INFO) << "Connection terminated: " << base::NetworkChannel::errorToString(error_code);

    // Show an error to the user.
    status_window_proxy_->onHostDisconnected(error_code);

    if (session_state_->isAutoReconnect())
    {
        LOG(LS_INFO) << "Reconnect to host enabled";
        session_state_->setReconnecting(true);

        if (!timeout_timer_)
        {
            timeout_timer_ = std::make_unique<base::WaitableTimer>(
                base::WaitableTimer::Type::SINGLE_SHOT, io_task_runner_);
            timeout_timer_->start(std::chrono::minutes(5), [this]()
            {
                LOG(LS_INFO) << "Reconnect timeout";

                if (session_state_->isConnectionByHostId() && !is_connected_to_router_)
                    status_window_proxy_->onWaitForRouterTimeout();
                else
                    status_window_proxy_->onWaitForHostTimeout();

                session_state_->setReconnecting(false);
                session_state_->setAutoReconnect(false);

                if (channel_)
                {
                    LOG(LS_INFO) << "Destroy channel";
                    channel_->setListener(nullptr);
                    channel_.reset();
                }

                if (router_controller_)
                {
                    LOG(LS_INFO) << "Destroy router controller";
                    router_controller_.reset();
                }
            });
        }

        // Delete old channel.
        if (channel_)
        {
            LOG(LS_INFO) << "Post task to destroy channel";
            channel_->setListener(nullptr);
            io_task_runner_->deleteSoon(std::move(channel_));
        }

        if (session_state_->isConnectionByHostId())
        {
            // If you are using an ID connection, then start the connection immediately. The Router
            // will notify you when the Host comes online again.
            state_ = State::CREATED;
            start();
        }
        else
        {
            status_window_proxy_->onWaitForHost();
            delayedReconnectToHost();
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpMessageReceived(uint8_t channel_id, const base::ByteArray& buffer)
{
    if (channel_id == proto::HOST_CHANNEL_ID_SESSION)
    {
        onSessionMessageReceived(channel_id, buffer);
    }
    else if (channel_id == proto::HOST_CHANNEL_ID_SERVICE)
    {
        // TODO
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled incoming message from channel: " << channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpMessageWritten(uint8_t channel_id, base::ByteArray&& buffer, size_t pending)
{
    serializer_.addBuffer(std::move(buffer));

    if (channel_id == proto::HOST_CHANNEL_ID_SESSION)
    {
        onSessionMessageWritten(channel_id, pending);
    }
    else if (channel_id == proto::HOST_CHANNEL_ID_SERVICE)
    {
        // TODO
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled outgoing message from channel: " << channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onRouterConnected(const base::Version& router_version)
{
    LOG(LS_INFO) << "Router connected";
    session_state_->setRouterVersion(router_version);
    status_window_proxy_->onRouterConnected();
    is_connected_to_router_ = true;
}

//--------------------------------------------------------------------------------------------------
void Client::onHostAwaiting()
{
    LOG(LS_INFO) << "Host awaiting";
    status_window_proxy_->onWaitForHost();
}

//--------------------------------------------------------------------------------------------------
void Client::onHostConnected(std::unique_ptr<base::TcpChannel> channel)
{
    LOG(LS_INFO) << "Host connected";
    DCHECK(channel);

    channel_ = std::move(channel);
    channel_->setListener(this);

    startAuthentication();

    // Router controller is no longer needed.
    LOG(LS_INFO) << "Post task to destroy router controller";
    io_task_runner_->deleteSoon(std::move(router_controller_));
    is_connected_to_router_ = false;
}

//--------------------------------------------------------------------------------------------------
void Client::onErrorOccurred(const RouterController::Error& error)
{
    status_window_proxy_->onRouterError(error);

    LOG(LS_INFO) << "Post task to destroy router controller";
    io_task_runner_->deleteSoon(std::move(router_controller_));
    is_connected_to_router_ = false;

    if (error.type == RouterController::ErrorType::NETWORK && session_state_->isReconnecting())
    {
        status_window_proxy_->onWaitForRouter();
        delayedReconnectToRouter();
    }
}

//--------------------------------------------------------------------------------------------------
void Client::startAuthentication()
{
    LOG(LS_INFO) << "Start authentication for '" << session_state_->hostUserName() << "'";

    session_state_->setReconnecting(false);
    reconnect_timer_.reset();
    timeout_timer_.reset();

    static const size_t kReadBufferSize = 2 * 1024 * 1024; // 2 Mb.

    channel_->setReadBufferSize(kReadBufferSize);
    channel_->setNoDelay(true);
    channel_->setKeepAlive(true);

    authenticator_ = std::make_unique<base::ClientAuthenticator>(io_task_runner_);

    authenticator_->setIdentify(proto::IDENTIFY_SRP);
    authenticator_->setUserName(session_state_->hostUserName());
    authenticator_->setPassword(session_state_->hostPassword());
    authenticator_->setSessionType(static_cast<uint32_t>(session_state_->sessionType()));
    authenticator_->setDisplayName(session_state_->displayName());

    authenticator_->start(std::move(channel_),
                          [this](base::ClientAuthenticator::ErrorCode error_code)
    {
        if (error_code == base::ClientAuthenticator::ErrorCode::SUCCESS)
        {
            LOG(LS_INFO) << "Successful authentication";

            // The authenticator takes the listener on itself, we return the receipt of
            // notifications.
            channel_ = authenticator_->takeChannel();
            channel_->setListener(this);

            const base::Version& host_version = authenticator_->peerVersion();
            session_state_->setHostVersion(host_version);

            if (host_version >= base::Version::kVersion_2_6_0)
            {
                LOG(LS_INFO) << "Using channel id support";
                channel_->setChannelIdSupport(true);
            }

            const base::Version& client_version = base::Version::kCurrentFullVersion;
            if (host_version > client_version)
            {
                LOG(LS_ERROR) << "Version mismatch (host: " << host_version.toString()
                              << " client: " << client_version.toString();
                status_window_proxy_->onVersionMismatch();
            }
            else
            {
                status_window_proxy_->onHostConnected();

                // Signal that everything is ready to start the session (connection established,
                // authentication passed).
                onSessionStarted();

                // Now the session will receive incoming messages.
                channel_->resume();
            }
        }
        else
        {
            LOG(LS_INFO) << "Failed authentication: "
                         << base::ClientAuthenticator::errorToString(error_code);
            status_window_proxy_->onAccessDenied(error_code);
        }

        // Authenticator is no longer needed.
        io_task_runner_->deleteSoon(std::move(authenticator_));
    });
}

//--------------------------------------------------------------------------------------------------
void Client::delayedReconnectToRouter()
{
    reconnect_timer_ = std::make_unique<base::WaitableTimer>(
        base::WaitableTimer::Type::SINGLE_SHOT, io_task_runner_);

    reconnect_timer_->start(std::chrono::seconds(5), [this]()
    {
        LOG(LS_INFO) << "Reconnecting to router";
        state_ = State::CREATED;
        start();
    });
}

//--------------------------------------------------------------------------------------------------
void Client::delayedReconnectToHost()
{
    reconnect_timer_ = std::make_unique<base::WaitableTimer>(
        base::WaitableTimer::Type::SINGLE_SHOT, io_task_runner_);

    reconnect_timer_->start(std::chrono::seconds(5), [this]()
    {
        LOG(LS_INFO) << "Reconnecting to host";
        state_ = State::CREATED;
        start();
    });
}

} // namespace client
