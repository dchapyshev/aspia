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

#include "build/version.h"
#include "client/status_window_proxy.h"
#include "common/message_serialization.h"
#include "net/network_channel_proxy.h"

namespace client {

Client::Client(std::shared_ptr<base::TaskRunner> ui_task_runner)
    : ui_task_runner_(std::move(ui_task_runner))
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
    channel_->send(common::serializeMessage(message));
}

void Client::onBeforeThreadRunning()
{
    // Initialize the task runner for IO.
    io_task_runner_ = io_thread_.taskRunner();

    // Show the status window.
    status_window_proxy_->onStarted(config_.address, config_.port);

    // Create a network channel for messaging.
    channel_ = std::make_unique<net::Channel>();

    // Set the listener for the network channel.
    channel_->setListener(this);

    // Now connect to the host.
    channel_->connect(config_.address, config_.port);
}

void Client::onAfterThreadRunning()
{
    authenticator_.reset();
    channel_.reset();

    if (session_started_)
    {
        // Session stopped.
        onSessionStopped();
    }
}

void Client::onConnected()
{
    channel_->setKeepAlive(true, std::chrono::minutes(1), std::chrono::seconds(1));
    channel_->setNoDelay(true);

    authenticator_ = std::make_unique<Authenticator>();

    authenticator_->setUserName(config_.username);
    authenticator_->setPassword(config_.password);
    authenticator_->setSessionType(config_.session_type);

    authenticator_->start(std::move(channel_), [this](Authenticator::ErrorCode error_code)
    {
        if (error_code == Authenticator::ErrorCode::SUCCESS)
        {
            // The authenticator takes the listener on itself, we return the receipt of
            // notifications.
            channel_ = authenticator_->takeChannel();
            channel_->setListener(this);

            status_window_proxy_->onConnected();

            session_started_ = true;

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
        io_task_runner_->deleteSoon(authenticator_.release());
    });
}

void Client::onDisconnected(net::ErrorCode error_code)
{
    // Show an error to the user.
    status_window_proxy_->onDisconnected(error_code);
}

} // namespace client
