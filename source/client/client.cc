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

#include "client/client.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "build/version.h"
#include "client/status_window_proxy.h"

#if defined(OS_MAC)
#include "base/mac/app_nap_blocker.h"
#endif // defined(OS_MAC)

namespace client {

Client::Client(std::shared_ptr<base::TaskRunner> io_task_runner)
    : io_task_runner_(std::move(io_task_runner))
{
    LOG(LS_INFO) << "Client Ctor";
    DCHECK(io_task_runner_);

#if defined(OS_MAC)
    base::addAppNapBlock();
#endif // defined(OS_MAC)
}

Client::~Client()
{
    LOG(LS_INFO) << "Client Dtor";
    DCHECK(io_task_runner_->belongsToCurrentThread());
    stop();

#if defined(OS_MAC)
    base::releaseAppNapBlock();
#endif // defined(OS_MAC)
}

void Client::start(const Config& config)
{
    DCHECK(io_task_runner_->belongsToCurrentThread());
    DCHECK(status_window_proxy_);

    if (state_ != State::CREATED)
    {
        LOG(LS_ERROR) << "Client already started before";
        return;
    }

    config_ = config;
    state_ = State::STARTED;

    if (base::isHostId(config_.address_or_id))
    {
        LOG(LS_INFO) << "Starting RELAY connection";

        // Show the status window.
        status_window_proxy_->onStarted(config_.address_or_id);

        router_controller_ =
            std::make_unique<RouterController>(config_.router_config.value(), io_task_runner_);
        router_controller_->connectTo(base::stringToHostId(config_.address_or_id), this);
    }
    else
    {
        LOG(LS_INFO) << "Starting DIRECT connection";

        // Show the status window.
        status_window_proxy_->onStarted(
            base::strCat({ config_.address_or_id, u":", base::numberToString16(config_.port) }));

        // Create a network channel for messaging.
        channel_ = std::make_unique<base::NetworkChannel>();

        // Set the listener for the network channel.
        channel_->setListener(this);

        // Now connect to the host.
        channel_->connect(config_.address_or_id, config_.port);
    }
}

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

        status_window_proxy_->onStopped();

        LOG(LS_INFO) << "Client stopped";
    }
}

void Client::setStatusWindow(std::shared_ptr<StatusWindowProxy> status_window_proxy)
{
    status_window_proxy_ = std::move(status_window_proxy);
}

// static
base::Version Client::version()
{
    return base::Version(ASPIA_VERSION_MAJOR, ASPIA_VERSION_MINOR, ASPIA_VERSION_PATCH);
}

std::u16string Client::computerName() const
{
    return config_.computer_name;
}

proto::SessionType Client::sessionType() const
{
    return config_.session_type;
}

void Client::sendMessage(const google::protobuf::MessageLite& message)
{
    if (!channel_)
    {
        LOG(LS_WARNING) << "sendMessage called but channel not initialized";
        return;
    }

    channel_->send(base::serialize(message));
}

int64_t Client::totalRx() const
{
    if (!channel_)
    {
        LOG(LS_WARNING) << "totalRx called but channel not initialized";
        return 0;
    }

    return channel_->totalRx();
}

int64_t Client::totalTx() const
{
    if (!channel_)
    {
        LOG(LS_WARNING) << "totalTx called but channel not initialized";
        return 0;
    }

    return channel_->totalTx();
}

int Client::speedRx()
{
    if (!channel_)
    {
        LOG(LS_WARNING) << "speedRx called but channel not initialized";
        return 0;
    }

    return channel_->speedRx();
}

int Client::speedTx()
{
    if (!channel_)
    {
        LOG(LS_WARNING) << "speedTx called but channel not initialized";
        return 0;
    }

    return channel_->speedTx();
}

void Client::onConnected()
{
    startAuthentication();
}

void Client::onDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    // Show an error to the user.
    status_window_proxy_->onDisconnected(error_code);
}

void Client::onHostConnected(std::unique_ptr<base::NetworkChannel> channel)
{
    DCHECK(channel);

    channel_ = std::move(channel);
    channel_->setListener(this);

    startAuthentication();

    // Router controller is no longer needed.
    io_task_runner_->deleteSoon(std::move(router_controller_));
}

void Client::onErrorOccurred(const RouterController::Error& error)
{
    status_window_proxy_->onRouterError(error);
}

void Client::startAuthentication()
{
    static const size_t kReadBufferSize = 2 * 1024 * 1024; // 2 Mb.

    channel_->setReadBufferSize(kReadBufferSize);
    channel_->setNoDelay(true);

    authenticator_ = std::make_unique<base::ClientAuthenticator>(io_task_runner_);

    authenticator_->setIdentify(proto::IDENTIFY_SRP);
    authenticator_->setUserName(config_.username);
    authenticator_->setPassword(config_.password);
    authenticator_->setSessionType(static_cast<uint32_t>(config_.session_type));

    authenticator_->start(std::move(channel_),
                          [this](base::ClientAuthenticator::ErrorCode error_code)
    {
        if (error_code == base::ClientAuthenticator::ErrorCode::SUCCESS)
        {
            // The authenticator takes the listener on itself, we return the receipt of
            // notifications.
            channel_ = authenticator_->takeChannel();
            channel_->setListener(this);

            if (authenticator_->peerVersion() >= base::Version(2, 0, 0))
            {
                LOG(LS_INFO) << "Using own keep alive";

                // Versions 2.0.0+ support their own implementation keep alive.
                channel_->setOwnKeepAlive(true);
            }
            else
            {
                LOG(LS_INFO) << "Using TCP keep alive";

                // Otherwise use TCP keep alive.
                channel_->setTcpKeepAlive(true);
            }

            status_window_proxy_->onConnected();

            // Signal that everything is ready to start the session (connection established,
            // authentication passed).
            onSessionStarted(authenticator_->peerVersion());

            // Now the session will receive incoming messages.
            channel_->resume();
        }
        else
        {
            status_window_proxy_->onAccessDenied(error_code);
        }

        // Authenticator is no longer needed.
        io_task_runner_->deleteSoon(std::move(authenticator_));
    });
}

} // namespace client
