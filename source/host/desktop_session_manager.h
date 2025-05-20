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

class DesktopSessionManager final : public QObject
{
    Q_OBJECT

public:
    static const int kDefaultScreenCaptureFps = 20;
    static const int kMinScreenCaptureFps = 1;
    static const int kMaxScreenCaptureFpsHighEnd = 30;
    static const int kMaxScreenCaptureFpsLowEnd = 20;

    explicit DesktopSessionManager(QObject* parent = nullptr);
    ~DesktopSessionManager() final;

    static int defaultCaptureFps();
    static int minCaptureFps();
    static int maxCaptureFps();

    void attachSession(const base::Location& location, base::SessionId session_id);
    void dettachSession(const base::Location& location);

    base::local_shared_ptr<DesktopSessionProxy> sessionProxy() const;

    bool isMouseLocked() const { return is_mouse_locked_; }
    void setMouseLock(bool enable);

    bool isKeyboardLocked() const { return is_keyboard_locked_; }
    void setKeyboardLock(bool enable);

    bool isPaused() const { return is_paused_; }
    void setPaused(bool enable);

public slots:
    void control(proto::internal::DesktopControl::Action action);
    void configure(const host::DesktopSession::Config& config);
    void selectScreen(const proto::Screen& screen);
    void captureScreen();
    void setScreenCaptureFps(int fps);

    void injectKeyEvent(const proto::KeyEvent& event);
    void injectTextEvent(const proto::TextEvent& event);
    void injectMouseEvent(const proto::MouseEvent& event);
    void injectTouchEvent(const proto::TouchEvent& event);
    void injectClipboardEvent(const proto::ClipboardEvent& event);

    void onNewIpcConnection();
    void onErrorOccurred();

signals:
    void sig_desktopSessionStarted();
    void sig_desktopSessionStopped();
    void sig_screenCaptured(const base::Frame* frame, const base::MouseCursor* mouse_cursor);
    void sig_screenCaptureError(proto::VideoErrorCode error_code);
    void sig_audioCaptured(const proto::AudioPacket& audio_packet);
    void sig_cursorPositionChanged(const proto::CursorPosition& cursor_position);
    void sig_screenListChanged(const proto::ScreenList& list);
    void sig_screenTypeChanged(const proto::ScreenType& type);
    void sig_clipboardEvent(const proto::ClipboardEvent& event);

private:
    enum class State { STOPPED, STARTING, STOPPING, DETACHED, ATTACHED };

    void setState(const base::Location& location, State state);
    void connectSessionSignals();
    static const char* stateToString(State state);

    QPointer<base::IpcServer> server_;
    QPointer<DesktopSession> session_;
    base::local_shared_ptr<DesktopSessionProxy> session_proxy_;
    QPointer<QTimer> session_attach_timer_;

    State state_ = State::STOPPED;
    base::SessionId session_id_ = base::kInvalidSessionId;

    bool is_mouse_locked_ = false;
    bool is_keyboard_locked_ = false;
    bool is_paused_ = false;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionManager);
};

} // namespace host

#endif // HOST_DESKTOP_SESSION_MANAGER_H
