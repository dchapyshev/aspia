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

#ifndef HOST__DESKTOP_SESSION_MANAGER_H
#define HOST__DESKTOP_SESSION_MANAGER_H

#include "base/session_id.h"
#include "base/waitable_timer.h"
#include "base/win/session_status.h"
#include "host/desktop_session.h"
#include "ipc/ipc_server.h"
#include "proto/desktop_internal.pb.h"

namespace base {
class Location;
} // namespace base

namespace desktop {
class Frame;
} // namespace desktop

namespace ipc {
class SharedMemory;
} // namespace ipc

namespace host {

class DesktopSessionProcess;
class DesktopSessionProxy;

class DesktopSessionManager
    : public ipc::Server::Delegate,
      public DesktopSession::Delegate
{
public:
    DesktopSessionManager(std::shared_ptr<base::TaskRunner> task_runner,
                          DesktopSession::Delegate* delegate);
    ~DesktopSessionManager();

    void attachSession(const base::Location& location, base::SessionId session_id);
    void dettachSession(const base::Location& location);

    std::shared_ptr<DesktopSessionProxy> sessionProxy() const;

protected:
    // ipc::Server::Delegate implementation.
    void onNewConnection(std::unique_ptr<ipc::Channel> channel) override;
    void onErrorOccurred() override;

    // DesktopSession::Delegate implementation.
    void onDesktopSessionStarted() override;
    void onDesktopSessionStopped() override;
    void onScreenCaptured(const desktop::Frame& frame) override;
    void onCursorCaptured(std::shared_ptr<desktop::MouseCursor> mouse_cursor) override;
    void onScreenListChanged(const proto::ScreenList& list) override;
    void onClipboardEvent(const proto::ClipboardEvent& event) override;

private:
    enum class State { STOPPED, STARTING, STOPPING, DETACHED, ATTACHED };

    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<ipc::Server> server_;
    std::unique_ptr<DesktopSession> session_;
    std::shared_ptr<DesktopSessionProxy> session_proxy_;
    base::WaitableTimer session_attach_timer_;

    State state_ = State::STOPPED;
    DesktopSession::Delegate* delegate_;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionManager);
};

} // namespace host

#endif // HOST__DESKTOP_SESSION_MANAGER_H
