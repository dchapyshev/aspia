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
#include "build/version.h"
#include "client/status_window_proxy.h"

namespace client {

Client::Client(std::shared_ptr<base::TaskRunner> io_task_runner)
    : io_task_runner_(std::move(io_task_runner))
{
    DCHECK(io_task_runner_);
}

Client::~Client()
{
    DCHECK(io_task_runner_->belongsToCurrentThread());
    stop();
}

void Client::start(const Config& config)
{
    DCHECK(io_task_runner_->belongsToCurrentThread());
    DCHECK(status_window_proxy_);

    config_ = config;

    // Show the status window.
    status_window_proxy_->onStarted(config_.address, config_.port);

    // Create a network channel for messaging.
    channel_ = std::make_unique<net::Channel>();

    // Set the listener for the network channel.
    channel_->setListener(this);

    // Now connect to the host.
    channel_->connect(config_.address, config_.port);
}

void Client::stop()
{
    DCHECK(io_task_runner_->belongsToCurrentThread());

    authenticator_.reset();
    channel_.reset();

    status_window_proxy_->onStopped();
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
    channel_->send(base::serialize(message));
}

int64_t Client::totalRx() const
{
    return channel_->totalRx();
}

int64_t Client::totalTx() const
{
    return channel_->totalTx();
}

int Client::speedRx()
{
    return channel_->speedRx();
}

int Client::speedTx()
{
    return channel_->speedTx();
}

void Client::onConnected()
{
    static const size_t kReadBufferSize = 2 * 1024 * 1024; // 2 Mb.
    static const std::chrono::minutes kKeepAliveTime{ 1 };
    static const std::chrono::seconds kKeepAliveInterval{ 3 };

    channel_->setReadBufferSize(kReadBufferSize);
    channel_->setKeepAlive(true, kKeepAliveTime, kKeepAliveInterval);
    channel_->setNoDelay(true);

    authenticator_ = std::make_unique<net::ClientAuthenticator>();

    authenticator_->setIdentify(proto::IDENTIFY_SRP);
    authenticator_->setUserName(config_.username);
    authenticator_->setPassword(config_.password);
    authenticator_->setSessionType(config_.session_type);

    authenticator_->start(std::move(channel_),
                          [this](net::ClientAuthenticator::ErrorCode error_code)
    {
        if (error_code == net::ClientAuthenticator::ErrorCode::SUCCESS)
        {
            // The authenticator takes the listener on itself, we return the receipt of
            // notifications.
            channel_ = authenticator_->takeChannel();
            channel_->setListener(this);

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

void Client::onDisconnected(net::Channel::ErrorCode error_code)
{
    // Show an error to the user.
    status_window_proxy_->onDisconnected(error_code);
}

} // namespace client
