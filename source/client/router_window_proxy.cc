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

#include "client/router_window_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "client/router_window.h"
#include "proto/router_admin.pb.h"

namespace client {

//--------------------------------------------------------------------------------------------------
RouterWindowProxy::RouterWindowProxy(std::shared_ptr<base::TaskRunner> ui_task_runner,
                                     RouterWindow* router_window)
    : ui_task_runner_(std::move(ui_task_runner)),
      router_window_(router_window)
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(ui_task_runner_ && router_window_);
}

//--------------------------------------------------------------------------------------------------
RouterWindowProxy::~RouterWindowProxy()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK(!router_window_);
}

//--------------------------------------------------------------------------------------------------
void RouterWindowProxy::dettach()
{
    LOG(LS_INFO) << "Dettach router window";
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    router_window_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void RouterWindowProxy::onConnecting()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&RouterWindowProxy::onConnecting, shared_from_this()));
        return;
    }

    if (router_window_)
        router_window_->onConnecting();
}

//--------------------------------------------------------------------------------------------------
void RouterWindowProxy::onConnected(const base::Version& peer_version)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&RouterWindowProxy::onConnected, shared_from_this(), peer_version));
        return;
    }

    if (router_window_)
        router_window_->onConnected(peer_version);
}

//--------------------------------------------------------------------------------------------------
void RouterWindowProxy::onDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&RouterWindowProxy::onDisconnected, shared_from_this(), error_code));
        return;
    }

    if (router_window_)
        router_window_->onDisconnected(error_code);
}

//--------------------------------------------------------------------------------------------------
void RouterWindowProxy::onWaitForRouter()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&RouterWindowProxy::onWaitForRouter, shared_from_this()));
        return;
    }

    if (router_window_)
        router_window_->onWaitForRouter();
}

//--------------------------------------------------------------------------------------------------
void RouterWindowProxy::onWaitForRouterTimeout()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&RouterWindowProxy::onWaitForRouterTimeout, shared_from_this()));
        return;
    }

    if (router_window_)
        router_window_->onWaitForRouterTimeout();
}

//--------------------------------------------------------------------------------------------------
void RouterWindowProxy::onVersionMismatch(const base::Version& router, const base::Version& client)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&RouterWindowProxy::onVersionMismatch, shared_from_this(), router, client));
        return;
    }

    if (router_window_)
        router_window_->onVersionMismatch(router, client);
}

//--------------------------------------------------------------------------------------------------
void RouterWindowProxy::onAccessDenied(base::Authenticator::ErrorCode error_code)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&RouterWindowProxy::onAccessDenied, shared_from_this(), error_code));
        return;
    }

    if (router_window_)
        router_window_->onAccessDenied(error_code);
}

//--------------------------------------------------------------------------------------------------
void RouterWindowProxy::onSessionList(std::shared_ptr<proto::SessionList> session_list)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&RouterWindowProxy::onSessionList, shared_from_this(), session_list));
        return;
    }

    if (router_window_)
        router_window_->onSessionList(session_list);
}

//--------------------------------------------------------------------------------------------------
void RouterWindowProxy::onSessionResult(std::shared_ptr<proto::SessionResult> session_result)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&RouterWindowProxy::onSessionResult, shared_from_this(), session_result));
        return;
    }

    if (router_window_)
        router_window_->onSessionResult(session_result);
}

//--------------------------------------------------------------------------------------------------
void RouterWindowProxy::onUserList(std::shared_ptr<proto::UserList> user_list)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&RouterWindowProxy::onUserList, shared_from_this(), user_list));
        return;
    }

    if (router_window_)
        router_window_->onUserList(user_list);
}

//--------------------------------------------------------------------------------------------------
void RouterWindowProxy::onUserResult(std::shared_ptr<proto::UserResult> user_result)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&RouterWindowProxy::onUserResult, shared_from_this(), user_result));
        return;
    }

    if (router_window_)
        router_window_->onUserResult(user_result);
}

} // namespace client
