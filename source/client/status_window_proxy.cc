//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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
void StatusWindowProxy::onStarted(const std::u16string& address_or_id)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&StatusWindowProxy::onStarted, shared_from_this(), address_or_id));
        return;
    }

    if (status_window_)
        status_window_->onStarted(address_or_id);
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
void StatusWindowProxy::onConnected()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&StatusWindowProxy::onConnected, shared_from_this()));
        return;
    }

    if (status_window_)
        status_window_->onConnected();
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::onDisconnected(base::TcpChannel::ErrorCode error_code)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&StatusWindowProxy::onDisconnected, shared_from_this(), error_code));
        return;
    }

    if (status_window_)
        status_window_->onDisconnected(error_code);
}

//--------------------------------------------------------------------------------------------------
void StatusWindowProxy::onVersionMismatch(const base::Version& host, const base::Version& client)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&StatusWindowProxy::onVersionMismatch, shared_from_this(), host, client));
        return;
    }

    if (status_window_)
        status_window_->onVersionMismatch(host, client);
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
