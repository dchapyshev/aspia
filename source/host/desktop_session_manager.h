//
// SmartCafe Project
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

#include <QPointer>
#include <QTimer>

#include "base/session_id.h"
#include "base/ipc/ipc_server.h"
#include "host/desktop_session.h"
#include "proto/desktop_internal.h"

namespace base {
class Frame;
class Location;
class SharedMemory;
} // namespace base

namespace host {

class DesktopSessionManager final : public QObject
{
    Q_OBJECT

public:
    static const int kDefaultScreenCaptureFps = 24;
    static const int kMinScreenCaptureFps = 1;
    static const int kMaxScreenCaptureFpsHighEnd = 30;
    static const int kMaxScreenCaptureFpsLowEnd = 20;

    enum class State { STOPPED, STARTING, STOPPING, DETACHED, ATTACHED };
    Q_ENUM(State)

    explicit DesktopSessionManager(QObject* parent = nullptr);
    ~DesktopSessionManager() final;

    static int defaultCaptureFps();
    static int minCaptureFps();
    static int maxCaptureFps();

    void attachSession(const base::Location& location, base::SessionId session_id);
    void dettachSession(const base::Location& location);

    bool isMouseLocked() const { return is_mouse_locked_; }
    void setMouseLock(bool enable);

    bool isKeyboardLocked() const { return is_keyboard_locked_; }
    void setKeyboardLock(bool enable);

    bool isPaused() const { return is_paused_; }
    void setPaused(bool enable);

public slots:
    void control(proto::internal::DesktopControl::Action action);
    void configure(const host::DesktopSession::Config& config);
    void selectScreen(const proto::desktop::Screen& screen);
    void captureScreen();
    void setScreenCaptureFps(int fps);

    void injectKeyEvent(const proto::desktop::KeyEvent& event);
    void injectTextEvent(const proto::desktop::TextEvent& event);
    void injectMouseEvent(const proto::desktop::MouseEvent& event);
    void injectTouchEvent(const proto::desktop::TouchEvent& event);
    void injectClipboardEvent(const proto::desktop::ClipboardEvent& event);

    void onNewIpcConnection();
    void onErrorOccurred();

signals:
    void sig_desktopSessionStarted();
    void sig_desktopSessionStopped();
    void sig_screenCaptured(const base::Frame* frame, const base::MouseCursor* mouse_cursor);
    void sig_screenCaptureError(proto::desktop::VideoErrorCode error_code);
    void sig_audioCaptured(const proto::desktop::AudioPacket& audio_packet);
    void sig_cursorPositionChanged(const proto::desktop::CursorPosition& cursor_position);
    void sig_screenListChanged(const proto::desktop::ScreenList& list);
    void sig_screenTypeChanged(const proto::desktop::ScreenType& type);
    void sig_clipboardEvent(const proto::desktop::ClipboardEvent& event);

private:
    void setState(const base::Location& location, State state);
    void startDesktopSession();

    QPointer<base::IpcServer> ipc_server_;
    QPointer<DesktopSession> session_;
    QPointer<QTimer> session_attach_timer_;

    State state_ = State::STOPPED;
    base::SessionId session_id_ = base::kInvalidSessionId;

    bool is_mouse_locked_ = false;
    bool is_keyboard_locked_ = false;
    bool is_paused_ = false;

    Q_DISABLE_COPY(DesktopSessionManager)
};

} // namespace host

#endif // HOST_DESKTOP_SESSION_MANAGER_H
