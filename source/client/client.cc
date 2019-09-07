//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "build/version.h"
#include "client/status_window_proxy.h"
#include "common/message_serialization.h"
#include "net/network_channel_proxy.h"

namespace client {

Client::Client(std::shared_ptr<base::TaskRunner> ui_task_runner)
    : ui_task_runner_(ui_task_runner)
{
    DCHECK(ui_task_runner_);
    DCHECK(ui_task_runner_->belongsToCurrentThread());
}

Client::~Client()
{
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    stop();
}

bool Client::start(const Config& config)
{
    DCHECK(ui_task_runner_->belongsToCurrentThread());

    config_ = config;

    if (!status_window_proxy_)
    {
        DLOG(LS_ERROR) << "Attempt to start the client without status window";
        return false;
    }

    // Start the thread for IO.
    io_thread_.start(base::MessageLoop::Type::ASIO, this);
    return true;
}

void Client::stop()
{
    DCHECK(ui_task_runner_->belongsToCurrentThread());

    io_thread_.stop();
}

void Client::setStatusWindow(StatusWindow* status_window)
{
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    DCHECK(!status_window_proxy_);

    status_window_proxy_ = StatusWindowProxy::create(ui_task_runner_, status_window);
}

// static
base::Version Client::version()
{
    return base::Version(ASPIA_VERSION_MAJOR, ASPIA_VERSION_MINOR, ASPIA_VERSION_PATCH);
}

std::shared_ptr<base::TaskRunner> Client::ioTaskRunner() const
{
    return io_task_runner_;
}

std::shared_ptr<base::TaskRunner> Client::uiTaskRunner() const
{
    return ui_task_runner_;
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
    channel_proxy_->send(common::serializeMessage(message));
}

void Client::onBeforeThreadRunning()
{
    // Initialize the task runner for IO.
    io_task_runner_ = io_thread_.taskRunner();

    // Show the status window.
    status_window_proxy_->onStarted(config_.address, config_.port);

    // Create a network channel for messaging.
    channel_ = std::make_unique<net::Channel>();
    channel_proxy_ = channel_->channelProxy();

    // Set the listener for the network channel.
    channel_proxy_->setListener(this);

    // Now connect to the host.
    channel_proxy_->connect(config_.address, config_.port);
}

void Client::onAfterThreadRunning()
{
    channel_.reset();
}

void Client::onConnected()
{
    channel_proxy_->setKeepAlive(true, std::chrono::minutes(1), std::chrono::seconds(1));
    channel_proxy_->setNoDelay(true);

    authenticator_ = std::make_unique<Authenticator>();

    authenticator_->setUserName(config_.username);
    authenticator_->setPassword(config_.password);
    authenticator_->setSessionType(config_.session_type);

    authenticator_->start(channel_proxy_, [this](Authenticator::ErrorCode error_code)
    {
        // Authenticator is no longer needed.
        std::unique_ptr<Authenticator> authenticator(std::move(authenticator_));

        // The authenticator takes the listener on itself, we return the receipt of notifications.
        channel_proxy_->setListener(this);

        if (error_code != Authenticator::ErrorCode::SUCCESS)
        {
            status_window_proxy_->onAccessDenied(error_code);
            return;
        }

        status_window_proxy_->onConnected();

        // Signal that everything is ready to start the session (connection established,
        // authentication passed).
        onSessionStarted(authenticator->peerVersion());

        // Now the session will receive incoming messages.
        channel_proxy_->resume();
    });
}

void Client::onDisconnected(net::ErrorCode error_code)
{
    // Session stopped.
    onSessionStopped();

    // Show an error to the user.
    status_window_proxy_->onDisconnected(error_code);
}

} // namespace client
