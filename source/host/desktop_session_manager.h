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

#ifndef HOST_DESKTOP_SESSION_MANAGER_H
#define HOST_DESKTOP_SESSION_MANAGER_H

#include "base/session_id.h"
#include "base/ipc/ipc_server.h"
#include "base/memory/local_memory.h"
#include "host/desktop_session.h"
#include "proto/desktop_internal.pb.h"

#include <QPointer>
#include <QTimer>

namespace base {
class Frame;
class Location;
class SharedMemory;
} // namespace base

namespace host {

class DesktopSessionProcess;
class DesktopSessionProxy;

class DesktopSessionManager final
    : public QObject,
      public DesktopSession::Delegate
{
    Q_OBJECT

public:
    explicit DesktopSessionManager(DesktopSession::Delegate* delegate, QObject* parent = nullptr);
    ~DesktopSessionManager() final;

    void attachSession(const base::Location& location, base::SessionId session_id);
    void dettachSession(const base::Location& location);

    base::local_shared_ptr<DesktopSessionProxy> sessionProxy() const;

public slots:
    void onNewIpcConnection();
    void onErrorOccurred();

protected:
    // DesktopSession::Delegate implementation.
    void onDesktopSessionStarted() final;
    void onDesktopSessionStopped() final;
    void onScreenCaptured(const base::Frame* frame, const base::MouseCursor* mouse_cursor) final;
    void onScreenCaptureError(proto::VideoErrorCode error_code) final;
    void onAudioCaptured(const proto::AudioPacket& audio_packet) final;
    void onCursorPositionChanged(const proto::CursorPosition& cursor_position) final;
    void onScreenListChanged(const proto::ScreenList& list) final;
    void onScreenTypeChanged(const proto::ScreenType& type) final;
    void onClipboardEvent(const proto::ClipboardEvent& event) final;

private:
    enum class State { STOPPED, STARTING, STOPPING, DETACHED, ATTACHED };
    void setState(const base::Location& location, State state);
    static const char* stateToString(State state);

    QPointer<base::IpcServer> server_;
    std::unique_ptr<DesktopSession> session_;
    base::local_shared_ptr<DesktopSessionProxy> session_proxy_;
    QTimer session_attach_timer_;

    State state_ = State::STOPPED;
    DesktopSession::Delegate* delegate_;

    base::SessionId session_id_ = base::kInvalidSessionId;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionManager);
};

} // namespace host

#endif // HOST_DESKTOP_SESSION_MANAGER_H
