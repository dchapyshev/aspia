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

#include "host/desktop_session_manager.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "desktop/desktop_frame.h"
#include "host/desktop_session_fake.h"
#include "host/desktop_session_ipc.h"
#include "host/desktop_session_process.h"
#include "host/desktop_session_proxy.h"
#include "ipc/ipc_channel.h"

namespace host {

DesktopSessionManager::DesktopSessionManager(
    std::shared_ptr<base::TaskRunner> task_runner, DesktopSession::Delegate* delegate)
    : task_runner_(task_runner),
      session_attach_timer_(task_runner),
      session_proxy_(std::make_shared<DesktopSessionProxy>()),
      delegate_(delegate)
{
    // Nothing
}

DesktopSessionManager::~DesktopSessionManager()
{
    state_ = State::STOPPING;
    dettachSession();
}

void DesktopSessionManager::attachSession(base::win::SessionId session_id)
{
    if (state_ == State::STOPPED)
    {
        session_attach_timer_.start(
            std::chrono::minutes(1), std::bind(&DesktopSessionManager::onErrorOccurred, this));
    }

    state_ = State::STARTING;

    std::u16string channel_id = ipc::Server::createUniqueId();

    server_ = std::make_unique<ipc::Server>();
    if (!server_->start(channel_id, this))
    {
        onErrorOccurred();
        return;
    }

    std::unique_ptr<DesktopSessionProcess> process =
        DesktopSessionProcess::create(session_id, channel_id);
    if (!process)
    {
        onErrorOccurred();
        return;
    }
}

void DesktopSessionManager::dettachSession()
{
    if (state_ == State::STOPPED || state_ == State::DETACHED)
        return;

    if (state_ != State::STOPPING)
        state_ = State::DETACHED;

    session_attach_timer_.stop();
    session_proxy_->dettach();
    session_.reset();

    LOG(LS_INFO) << "Session process is detached";

    if (state_ == State::STOPPING)
        return;

    session_attach_timer_.start(
        std::chrono::minutes(1), std::bind(&DesktopSessionManager::onErrorOccurred, this));

    // The real session process has ended. We create a temporary fake session.
    session_ = std::make_unique<DesktopSessionFake>(this);
    session_proxy_->attach(session_.get());
    session_->start();
}

std::shared_ptr<DesktopSessionProxy> DesktopSessionManager::sessionProxy() const
{
    return session_proxy_;
}

void DesktopSessionManager::onNewConnection(std::unique_ptr<ipc::Channel> channel)
{
    session_attach_timer_.stop();
    task_runner_->deleteSoon(server_.release());

    session_ = std::make_unique<DesktopSessionIpc>(std::move(channel), this);
    session_proxy_->attach(session_.get());

    state_ = State::ATTACHED;
    session_->start();
}

void DesktopSessionManager::onErrorOccurred()
{
    if (state_ == State::STOPPED || state_ == State::STOPPING)
        return;

    state_ = State::STOPPING;
    dettachSession();
    state_ = State::STOPPED;
}

void DesktopSessionManager::onDesktopSessionStarted()
{
    delegate_->onDesktopSessionStarted();
}

void DesktopSessionManager::onDesktopSessionStopped()
{
    dettachSession();
}

void DesktopSessionManager::onScreenCaptured(const desktop::Frame& frame)
{
    delegate_->onScreenCaptured(frame);
}

void DesktopSessionManager::onCursorCaptured(std::shared_ptr<desktop::MouseCursor> mouse_cursor)
{
    delegate_->onCursorCaptured(std::move(mouse_cursor));
}

void DesktopSessionManager::onScreenListChanged(const proto::ScreenList& list)
{
    delegate_->onScreenListChanged(list);
}

void DesktopSessionManager::onClipboardEvent(const proto::ClipboardEvent& event)
{
    delegate_->onClipboardEvent(event);
}

} // namespace host
