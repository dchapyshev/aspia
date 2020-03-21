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

#include "host/user_session_window_proxy.h"

#include "base/logging.h"
#include "host/user_session_window.h"

namespace host {

UserSessionWindowProxy::UserSessionWindowProxy(
    std::shared_ptr<base::TaskRunner> ui_task_runner, UserSessionWindow* window)
    : ui_task_runner_(std::move(ui_task_runner)),
      window_(window)
{
    DCHECK(ui_task_runner_ && ui_task_runner_->belongsToCurrentThread());
    DCHECK(window_);
}

UserSessionWindowProxy::~UserSessionWindowProxy()
{
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    DCHECK(!window_);
}

void UserSessionWindowProxy::dettach()
{
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    window_ = nullptr;
}

void UserSessionWindowProxy::onStateChanged(UserSessionAgent::State state)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(
            &UserSessionWindowProxy::onStateChanged, shared_from_this(), state));
        return;
    }

    if (window_)
        window_->onStateChanged(state);
}

void UserSessionWindowProxy::onClientListChanged(const UserSessionAgent::ClientList& clients)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(
            &UserSessionWindowProxy::onClientListChanged, shared_from_this(), clients));
        return;
    }

    if (window_)
        window_->onClientListChanged(clients);
}

void UserSessionWindowProxy::onCredentialsChanged(const proto::internal::Credentials& credentials)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(
            &UserSessionWindowProxy::onCredentialsChanged, shared_from_this(), credentials));
        return;
    }

    if (window_)
        window_->onCredentialsChanged(credentials);
}

} // namespace host
