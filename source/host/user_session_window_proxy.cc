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

#include "host/user_session_window_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "host/user_session_window.h"

namespace host {

//--------------------------------------------------------------------------------------------------
UserSessionWindowProxy::UserSessionWindowProxy(
    std::shared_ptr<base::TaskRunner> ui_task_runner, UserSessionWindow* window)
    : ui_task_runner_(std::move(ui_task_runner)),
      window_(window)
{
    DCHECK(ui_task_runner_ && ui_task_runner_->belongsToCurrentThread());
    DCHECK(window_);
}

//--------------------------------------------------------------------------------------------------
UserSessionWindowProxy::~UserSessionWindowProxy()
{
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    DCHECK(!window_);
}

//--------------------------------------------------------------------------------------------------
void UserSessionWindowProxy::dettach()
{
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    window_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void UserSessionWindowProxy::onStatusChanged(UserSessionAgent::Status status)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(
            &UserSessionWindowProxy::onStatusChanged, shared_from_this(), status));
        return;
    }

    if (window_)
        window_->onStatusChanged(status);
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
void UserSessionWindowProxy::onRouterStateChanged(const proto::internal::RouterState& state)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(
            &UserSessionWindowProxy::onRouterStateChanged, shared_from_this(), state));
        return;
    }

    if (window_)
        window_->onRouterStateChanged(state);
}

//--------------------------------------------------------------------------------------------------
void UserSessionWindowProxy::onConnectConfirmationRequest(
    const proto::internal::ConnectConfirmationRequest& request)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(
            &UserSessionWindowProxy::onConnectConfirmationRequest, shared_from_this(), request));
        return;
    }

    if (window_)
        window_->onConnectConfirmationRequest(request);
}

//--------------------------------------------------------------------------------------------------
void UserSessionWindowProxy::onVideoRecordingStateChanged(
    const std::string& computer_name, const std::string& user_name, bool started)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(
            &UserSessionWindowProxy::onVideoRecordingStateChanged,
            shared_from_this(),
            computer_name,
            user_name,
            started));
        return;
    }

    if (window_)
        window_->onVideoRecordingStateChanged(computer_name, user_name, started);
}

//--------------------------------------------------------------------------------------------------
void UserSessionWindowProxy::onTextChat(const proto::TextChat& text_chat)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(
            &UserSessionWindowProxy::onTextChat, shared_from_this(), text_chat));
        return;
    }

    if (window_)
        window_->onTextChat(text_chat);
}

} // namespace host
