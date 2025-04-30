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
#include "base/serialization.h"
#include "base/peer/client_authenticator.h"

#if defined(OS_MAC)
#include "base/mac/app_nap_blocker.h"
#endif // defined(OS_MAC)

namespace client {

namespace {

auto g_statusType = qRegisterMetaType<client::Client::Status>();

} // namespace

//--------------------------------------------------------------------------------------------------
Client::Client(QObject* parent)
    : QObject(parent),
      timeout_timer_(new QTimer(this)),
      reconnect_timer_(new QTimer(this))
{
    LOG(LS_INFO) << "Ctor";

    timeout_timer_->setSingleShot(true);
    connect(timeout_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(LS_INFO) << "Reconnect timeout";

        if (session_state_->isConnectionByHostId() && !is_connected_to_router_)
            emit sig_statusChanged(Status::WAIT_FOR_ROUTER_TIMEOUT);
        else
            emit sig_statusChanged(Status::WAIT_FOR_HOST_TIMEOUT);

        session_state_->setReconnecting(false);
        session_state_->setAutoReconnect(false);

        if (channel_)
        {
            LOG(LS_INFO) << "Destroy channel";
            channel_->deleteLater();
        }

        if (router_controller_)
        {
            LOG(LS_INFO) << "Destroy router controller";
            router_controller_->deleteLater();
        }
    });

    reconnect_timer_->setSingleShot(true);
    connect(reconnect_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(LS_INFO) << "Reconnecting...";
        state_ = State::CREATED;
        start();
    });

#if defined(OS_MAC)
    base::addAppNapBlock();
#endif // defined(OS_MAC)
}

//--------------------------------------------------------------------------------------------------
Client::~Client()
{
    LOG(LS_INFO) << "Dtor";

    state_ = State::STOPPPED;
    emit sig_statusChanged(Status::STOPPED);

#if defined(OS_MAC)
    base::releaseAppNapBlock();
#endif // defined(OS_MAC)
}

//--------------------------------------------------------------------------------------------------
void Client::start()
{
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

        router_controller_ = new RouterController(*config.router_config, this);

        connect(router_controller_, &RouterController::sig_routerConnected,
                this, &Client::onRouterConnected);
        connect(router_controller_, &RouterController::sig_hostAwaiting,
                this, &Client::onHostAwaiting);
        connect(router_controller_, &RouterController::sig_hostConnected,
                this, &Client::onHostConnected);
        connect(router_controller_, &RouterController::sig_errorOccurred,
                this, &Client::onRouterErrorOccurred);

        bool reconnecting = session_state_->isReconnecting();
        if (!reconnecting)
        {
            // Show the status window.
            emit sig_statusChanged(Status::STARTED);
        }

        emit sig_statusChanged(Status::ROUTER_CONNECTING);
        router_controller_->connectTo(base::stringToHostId(config.address_or_id), reconnecting);
    }
    else
    {
        LOG(LS_INFO) << "Starting DIRECT connection";

        if (!session_state_->isReconnecting())
        {
            // Show the status window.
            emit sig_statusChanged(Status::STARTED);
        }

        // Create a network channel for messaging.
        channel_ = new base::TcpChannel(this);

        connect(channel_, &base::TcpChannel::sig_connected, this, &Client::onTcpConnected);

        // Now connect to the host.
        emit sig_statusChanged(Status::HOST_CONNECTING);
        channel_->connect(config.address_or_id, config.port);
    }
}

//--------------------------------------------------------------------------------------------------
void Client::setSessionState(std::shared_ptr<SessionState> session_state)
{
    LOG(LS_INFO) << "Session state installed";
    session_state_ = session_state;
}

//--------------------------------------------------------------------------------------------------
// static
const char* Client::statusToString(Status status)
{
    switch (status)
    {
        case Status::STARTED:
            return "STARTED";
        case Status::STOPPED:
            return "STOPPED";
        case Status::ROUTER_CONNECTING:
            return "ROUTER_CONNECTING";
        case Status::ROUTER_CONNECTED:
            return "ROUTER_CONNECTED";
        case Status::ROUTER_ERROR:
            return "ROUTER_ERROR";
        case Status::HOST_CONNECTING:
            return "HOST_CONNECTING";
        case Status::HOST_CONNECTED:
            return "HOST_CONNECTED";
        case Status::HOST_DISCONNECTED:
            return "HOST_DISCONNECTED";
        case Status::WAIT_FOR_ROUTER:
            return "WAIT_FOR_ROUTER";
        case Status::WAIT_FOR_ROUTER_TIMEOUT:
            return "WAIT_FOR_ROUTER_TIMEOUT";
        case Status::WAIT_FOR_HOST:
            return "WAIT_FOR_HOST";
        case Status::WAIT_FOR_HOST_TIMEOUT:
            return "WAIT_FOR_HOST_TIMEOUT";
        case Status::VERSION_MISMATCH:
            return "VERSION_MISMATCH";
        case Status::ACCESS_DENIED:
            return "ACCESS_DENIED";
        default:
            return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
void Client::sendMessage(uint8_t channel_id, const google::protobuf::MessageLite& message)
{
    if (!channel_)
    {
        LOG(LS_ERROR) << "sendMessage called but channel not initialized";
        return;
    }

    channel_->send(channel_id, base::serialize(message));
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
    emit sig_statusChanged(Status::HOST_DISCONNECTED, QVariant::fromValue(error_code));

    if (session_state_->isAutoReconnect())
    {
        LOG(LS_INFO) << "Reconnect to host enabled";
        session_state_->setReconnecting(true);

        timeout_timer_->start(std::chrono::minutes(5));

        // Delete old channel.
        if (channel_)
        {
            LOG(LS_INFO) << "Post task to destroy channel";
            channel_->deleteLater();
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
            emit sig_statusChanged(Status::WAIT_FOR_HOST);
            delayedReconnect();
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpMessageReceived(uint8_t channel_id, const QByteArray& buffer)
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
void Client::onTcpMessageWritten(uint8_t channel_id, size_t pending)
{
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
    emit sig_statusChanged(Status::ROUTER_CONNECTED);
    is_connected_to_router_ = true;
}

//--------------------------------------------------------------------------------------------------
void Client::onHostAwaiting()
{
    LOG(LS_INFO) << "Host awaiting";
    emit sig_statusChanged(Status::WAIT_FOR_HOST);
}

//--------------------------------------------------------------------------------------------------
void Client::onHostConnected()
{
    LOG(LS_INFO) << "Host connected";

    if (!router_controller_)
    {
        LOG(LS_ERROR) << "No router controller instance";
        return;
    }

    channel_ = router_controller_->takeChannel();
    channel_->setParent(this);

    startAuthentication();

    // Router controller is no longer needed.
    LOG(LS_INFO) << "Post task to destroy router controller";
    router_controller_->deleteLater();
    is_connected_to_router_ = false;
}

//--------------------------------------------------------------------------------------------------
void Client::onRouterErrorOccurred(const RouterController::Error& error)
{
    emit sig_statusChanged(Status::ROUTER_ERROR, QVariant::fromValue(error));

    LOG(LS_INFO) << "Post task to destroy router controller";
    router_controller_->deleteLater();
    is_connected_to_router_ = false;

    if (error.type == RouterController::ErrorType::NETWORK && session_state_->isReconnecting())
    {
        emit sig_statusChanged(Status::WAIT_FOR_ROUTER);
        delayedReconnect();
    }
}

//--------------------------------------------------------------------------------------------------
void Client::startAuthentication()
{
    LOG(LS_INFO) << "Start authentication for '" << session_state_->hostUserName() << "'";

    session_state_->setReconnecting(false);
    reconnect_timer_->stop();
    timeout_timer_->stop();

    static const size_t kReadBufferSize = 2 * 1024 * 1024; // 2 Mb.

    channel_->setReadBufferSize(kReadBufferSize);
    channel_->setNoDelay(true);
    channel_->setKeepAlive(true);

    authenticator_ = new base::ClientAuthenticator(this);

    authenticator_->setIdentify(proto::IDENTIFY_SRP);
    authenticator_->setUserName(session_state_->hostUserName());
    authenticator_->setPassword(session_state_->hostPassword());
    authenticator_->setSessionType(static_cast<uint32_t>(session_state_->sessionType()));
    authenticator_->setDisplayName(session_state_->displayName());

    connect(authenticator_, &base::Authenticator::sig_finished,
            this, [this](base::Authenticator::ErrorCode error_code)
    {
        if (error_code == base::Authenticator::ErrorCode::SUCCESS)
        {
            LOG(LS_INFO) << "Successful authentication";

            connect(channel_, &base::TcpChannel::sig_disconnected,
                    this, &Client::onTcpDisconnected);
            connect(channel_, &base::TcpChannel::sig_messageReceived,
                    this, &Client::onTcpMessageReceived);
            connect(channel_, &base::TcpChannel::sig_messageWritten,
                    this, &Client::onTcpMessageWritten);

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
                emit sig_statusChanged(Status::VERSION_MISMATCH);
            }
            else
            {
                emit sig_statusChanged(Status::HOST_CONNECTED);

                // Signal that everything is ready to start the session (connection established,
                // authentication passed).
                onSessionStarted();

                emit sig_showSessionWindow();

                // Now the session will receive incoming messages.
                channel_->resume();
            }
        }
        else
        {
            LOG(LS_INFO) << "Failed authentication: "
                         << base::Authenticator::errorToString(error_code);
            emit sig_statusChanged(Status::ACCESS_DENIED, QVariant::fromValue(error_code));
        }

        // Authenticator is no longer needed.
        authenticator_->deleteLater();
    });

    authenticator_->start(std::move(channel_));
}

//--------------------------------------------------------------------------------------------------
void Client::delayedReconnect()
{
    reconnect_timer_->start(std::chrono::seconds(5));
}

} // namespace client
