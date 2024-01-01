//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/status_window_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"

namespace client {

//--------------------------------------------------------------------------------------------------
StatusWindowProxy::StatusWindowProxy(std::shared_ptr<base::TaskRunner> ui_task_runner,
                                     StatusWindow* status_window)
    : ui_task_runner_(std::move(ui_task_runner)),
      status_window_(status_window)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
StatusWindowProxy::~StatusWindowProxy()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK(!status_window_);
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::dettach()
{
    LOG(LS_INFO) << "Dettach status window";
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    status_window_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::onStarted()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&StatusWindowProxy::onStarted, shared_from_this()));
        return;
    }

    if (status_window_)
        status_window_->onStarted();
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::onStopped()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&StatusWindowProxy::onStopped, shared_from_this()));
        return;
    }

    if (status_window_)
    {
        status_window_->onStopped();
        status_window_ = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::onRouterConnecting()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&StatusWindowProxy::onRouterConnecting, shared_from_this()));
        return;
    }

    if (status_window_)
        status_window_->onRouterConnecting();
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::onRouterConnected()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&StatusWindowProxy::onRouterConnected, shared_from_this()));
        return;
    }

    if (status_window_)
        status_window_->onRouterConnected();
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::onHostConnecting()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&StatusWindowProxy::onHostConnecting, shared_from_this()));
        return;
    }

    if (status_window_)
        status_window_->onHostConnecting();
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::onHostConnected()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&StatusWindowProxy::onHostConnected, shared_from_this()));
        return;
    }

    if (status_window_)
        status_window_->onHostConnected();
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::onHostDisconnected(base::TcpChannel::ErrorCode error_code)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&StatusWindowProxy::onHostDisconnected, shared_from_this(), error_code));
        return;
    }

    if (status_window_)
        status_window_->onHostDisconnected(error_code);
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::onWaitForRouter()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&StatusWindowProxy::onWaitForRouter, shared_from_this()));
        return;
    }

    if (status_window_)
        status_window_->onWaitForRouter();
}

void StatusWindowProxy::onWaitForRouterTimeout()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&StatusWindowProxy::onWaitForRouterTimeout, shared_from_this()));
        return;
    }

    if (status_window_)
        status_window_->onWaitForRouterTimeout();
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::onWaitForHost()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&StatusWindowProxy::onWaitForHost, shared_from_this()));
        return;
    }

    if (status_window_)
        status_window_->onWaitForHost();
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::onWaitForHostTimeout()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&StatusWindowProxy::onWaitForHostTimeout, shared_from_this()));
        return;
    }

    if (status_window_)
        status_window_->onWaitForHostTimeout();
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::onVersionMismatch()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&StatusWindowProxy::onVersionMismatch, shared_from_this()));
        return;
    }

    if (status_window_)
        status_window_->onVersionMismatch();
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::onAccessDenied(base::ClientAuthenticator::ErrorCode error_code)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&StatusWindowProxy::onAccessDenied, shared_from_this(), error_code));
        return;
    }

    if (status_window_)
        status_window_->onAccessDenied(error_code);
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::onRouterError(const RouterController::Error& error)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&StatusWindowProxy::onRouterError, shared_from_this(), error));
        return;
    }

    if (status_window_)
        status_window_->onRouterError(error);
}

} // namespace client
