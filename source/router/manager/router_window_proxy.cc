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

#include "router/manager/router_window_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "proto/router.pb.h"
#include "router/manager/router_window.h"

namespace router {

RouterWindowProxy::RouterWindowProxy(std::shared_ptr<base::TaskRunner> ui_task_runner,
                                     RouterWindow* router_window)
    : ui_task_runner_(std::move(ui_task_runner)),
      router_window_(router_window)
{
    DCHECK(ui_task_runner_ && router_window_);
}

RouterWindowProxy::~RouterWindowProxy()
{
    DCHECK(!router_window_);
}

void RouterWindowProxy::dettach()
{
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    router_window_ = nullptr;
}

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

void RouterWindowProxy::onDisconnected(net::Channel::ErrorCode error_code)
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

void RouterWindowProxy::onAccessDenied(net::ClientAuthenticator::ErrorCode error_code)
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

void RouterWindowProxy::onPeerList(std::shared_ptr<proto::PeerList> peer_list)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&RouterWindowProxy::onPeerList, shared_from_this(), peer_list));
        return;
    }

    if (router_window_)
        router_window_->onPeerList(peer_list);
}

void RouterWindowProxy::onPeerResult(std::shared_ptr<proto::PeerResult> peer_result)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&RouterWindowProxy::onPeerResult, shared_from_this(), peer_result));
        return;
    }

    if (router_window_)
        router_window_->onPeerResult(peer_result);
}

void RouterWindowProxy::onProxyList(std::shared_ptr<proto::ProxyList> proxy_list)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&RouterWindowProxy::onProxyList, shared_from_this(), proxy_list));
        return;
    }

    if (router_window_)
        router_window_->onProxyList(proxy_list);
}

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

} // namespace router
