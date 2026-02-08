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

#include "client/client.h"

#include "base/logging.h"
#include "base/version_constants.h"
#include "base/peer/client_authenticator.h"

#if defined(Q_OS_MACOS)
#include "base/mac/app_nap_blocker.h"
#endif // defined(Q_OS_MACOS)

namespace client {

namespace {

auto g_statusType = qRegisterMetaType<client::Client::Status>();
static const int kReadBufferSize = 2 * 1024 * 1024; // 2 Mb.
static const int kWriteBufferSize = 2 * 1024 * 1024; // 2 Mb.

} // namespace

//--------------------------------------------------------------------------------------------------
Client::Client(QObject* parent)
    : QObject(parent),
      timeout_timer_(new QTimer(this)),
      reconnect_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    timeout_timer_->setSingleShot(true);
    connect(timeout_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(INFO) << "Reconnect timeout";

        if (session_state_->isConnectionByHostId() && !is_connected_to_router_)
            emit sig_statusChanged(Status::WAIT_FOR_ROUTER_TIMEOUT);
        else
            emit sig_statusChanged(Status::WAIT_FOR_HOST_TIMEOUT);

        session_state_->setReconnecting(false);
        session_state_->setAutoReconnect(false);

        if (tcp_channel_)
        {
            LOG(INFO) << "Destroy channel";
            tcp_channel_->deleteLater();
        }

        if (router_controller_)
        {
            LOG(INFO) << "Destroy router controller";
            router_controller_->deleteLater();
        }
    });

    reconnect_timer_->setSingleShot(true);
    connect(reconnect_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(INFO) << "Reconnecting...";
        state_ = State::CREATED;
        start();
    });

#if defined(Q_OS_MACOS)
    base::addAppNapBlock();
#endif // defined(Q_OS_MACOS)
}

//--------------------------------------------------------------------------------------------------
Client::~Client()
{
    LOG(INFO) << "Dtor";

    state_ = State::STOPPPED;
    emit sig_statusChanged(Status::STOPPED);

#if defined(Q_OS_MACOS)
    base::releaseAppNapBlock();
#endif // defined(Q_OS_MACOS)
}

//--------------------------------------------------------------------------------------------------
void Client::start()
{
    if (state_ != State::CREATED)
    {
        LOG(ERROR) << "Client already started before";
        return;
    }

    if (!session_state_)
    {
        LOG(ERROR) << "Session state not installed";
        return;
    }

    state_ = State::STARTED;
    is_connected_to_router_ = false;

    Config config = session_state_->config();

    std::unique_ptr<base::ClientAuthenticator> authenticator =
        std::make_unique<base::ClientAuthenticator>();

    authenticator->setIdentify(proto::key_exchange::IDENTIFY_SRP);
    authenticator->setUserName(session_state_->hostUserName());
    authenticator->setPassword(session_state_->hostPassword());
    authenticator->setSessionType(static_cast<quint32>(session_state_->sessionType()));
    authenticator->setDisplayName(session_state_->displayName());

    if (base::isHostId(config.address_or_id))
    {
        LOG(INFO) << "Starting RELAY connection";

        if (!config.router_config.has_value())
        {
            LOG(FATAL) << "No router config. Continuation is impossible";
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
        router_controller_->connectTo(
            base::stringToHostId(config.address_or_id), authenticator.release(), reconnecting);
    }
    else
    {
        LOG(INFO) << "Starting DIRECT connection";

        if (!session_state_->isReconnecting())
        {
            // Show the status window.
            emit sig_statusChanged(Status::STARTED);
        }

        // Create a network channel for messaging.
        tcp_channel_ = new base::TcpChannel(authenticator.release(), this);

        connect(tcp_channel_, &base::TcpChannel::sig_authenticated, this, &Client::onTcpReady);
        connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred, this, &Client::onTcpErrorOccurred);
        connect(tcp_channel_, &base::TcpChannel::sig_messageReceived, this, &Client::onTcpMessageReceived);
        connect(tcp_channel_, &base::TcpChannel::sig_messageWritten, this, &Client::onTcpMessageWritten);

        // Now connect to the host.
        emit sig_statusChanged(Status::HOST_CONNECTING);
        tcp_channel_->connectTo(config.address_or_id, config.port);
    }
}

//--------------------------------------------------------------------------------------------------
void Client::setSessionState(std::shared_ptr<SessionState> session_state)
{
    LOG(INFO) << "Session state installed";
    session_state_ = session_state;
}

//--------------------------------------------------------------------------------------------------
void Client::sendMessage(const QByteArray& message)
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "sendMessage called but channel not initialized";
        return;
    }

    tcp_channel_->send(proto::peer::CHANNEL_ID_SESSION, message);
}

//--------------------------------------------------------------------------------------------------
qint64 Client::totalRx() const
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "totalRx called but channel not initialized";
        return 0;
    }

    return tcp_channel_->totalRx();
}

//--------------------------------------------------------------------------------------------------
qint64 Client::totalTx() const
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "totalTx called but channel not initialized";
        return 0;
    }

    return tcp_channel_->totalTx();
}

//--------------------------------------------------------------------------------------------------
int Client::speedRx()
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "speedRx called but channel not initialized";
        return 0;
    }

    return tcp_channel_->speedRx();
}

//--------------------------------------------------------------------------------------------------
int Client::speedTx()
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "speedTx called but channel not initialized";
        return 0;
    }

    return tcp_channel_->speedTx();
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpReady()
{
    LOG(INFO) << "Connection established";
    channelReady();
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    LOG(INFO) << "Connection terminated:" << error_code;

    // Show an error to the user.
    emit sig_statusChanged(Status::HOST_DISCONNECTED, QVariant::fromValue(error_code));

    if (session_state_->isAutoReconnect())
    {
        LOG(INFO) << "Reconnect to host enabled";
        session_state_->setReconnecting(true);

        timeout_timer_->start(std::chrono::minutes(5));

        // Delete old channel.
        if (tcp_channel_)
        {
            LOG(INFO) << "Post task to destroy channel";
            tcp_channel_->deleteLater();
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
void Client::onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::peer::CHANNEL_ID_SESSION)
    {
        onSessionMessageReceived(buffer);
    }
    else if (channel_id == proto::peer::CHANNEL_ID_SERVICE)
    {
        // TODO
    }
    else
    {
        LOG(ERROR) << "Unhandled incoming message from channel:" << channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpMessageWritten(quint8 channel_id, size_t pending)
{
    if (channel_id == proto::peer::CHANNEL_ID_SESSION)
    {
        onSessionMessageWritten(pending);
    }
    else if (channel_id == proto::peer::CHANNEL_ID_SERVICE)
    {
        // TODO
    }
    else
    {
        LOG(ERROR) << "Unhandled outgoing message from channel:" << channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onRouterConnected(const QVersionNumber& router_version)
{
    LOG(INFO) << "Router connected";
    session_state_->setRouterVersion(router_version);
    emit sig_statusChanged(Status::ROUTER_CONNECTED);
    is_connected_to_router_ = true;
}

//--------------------------------------------------------------------------------------------------
void Client::onHostAwaiting()
{
    LOG(INFO) << "Host awaiting";
    emit sig_statusChanged(Status::WAIT_FOR_HOST);
}

//--------------------------------------------------------------------------------------------------
void Client::onHostConnected()
{
    LOG(INFO) << "Host connected";

    if (!router_controller_)
    {
        LOG(ERROR) << "No router controller instance";
        return;
    }

    tcp_channel_ = router_controller_->takeChannel();
    tcp_channel_->setParent(this);

    connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred, this, &Client::onTcpErrorOccurred);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived, this, &Client::onTcpMessageReceived);
    connect(tcp_channel_, &base::TcpChannel::sig_messageWritten, this, &Client::onTcpMessageWritten);

    channelReady();

    // Router controller is no longer needed.
    LOG(INFO) << "Post task to destroy router controller";
    router_controller_->deleteLater();
    is_connected_to_router_ = false;
}

//--------------------------------------------------------------------------------------------------
void Client::onRouterErrorOccurred(const RouterController::Error& error)
{
    emit sig_statusChanged(Status::ROUTER_ERROR, QVariant::fromValue(error));

    LOG(INFO) << "Post task to destroy router controller";
    router_controller_->deleteLater();
    is_connected_to_router_ = false;

    if (error.type == RouterController::ErrorType::NETWORK && session_state_->isReconnecting())
    {
        emit sig_statusChanged(Status::WAIT_FOR_ROUTER);
        delayedReconnect();
    }
}

//--------------------------------------------------------------------------------------------------
void Client::delayedReconnect()
{
    reconnect_timer_->start(std::chrono::seconds(5));
}

//--------------------------------------------------------------------------------------------------
void Client::channelReady()
{
    session_state_->setReconnecting(false);
    reconnect_timer_->stop();
    timeout_timer_->stop();

    const QVersionNumber& host_version = tcp_channel_->peerVersion();
    session_state_->setHostVersion(host_version);

    const QVersionNumber& client_version = base::kCurrentVersion;
    if (host_version > client_version)
    {
        LOG(ERROR) << "Version mismatch. Host:" << host_version << "Client:" << client_version;
        emit sig_statusChanged(Status::VERSION_MISMATCH);
        return;
    }

    // After authentication we already know the peer's computer name (sent by Host in handshake).
    emit sig_statusChanged(Status::HOST_CONNECTED, tcp_channel_ ? tcp_channel_->peerComputerName()
                                                               : QString());

    // Signal that everything is ready to start the session (connection established,
    // authentication passed).
    onSessionStarted();

    emit sig_showSessionWindow();

    // Now the session will receive incoming messages.
    tcp_channel_->setReadBufferSize(kReadBufferSize);
    tcp_channel_->setWriteBufferSize(kWriteBufferSize);
    tcp_channel_->resume();
}

} // namespace client
