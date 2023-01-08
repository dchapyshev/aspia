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

#ifndef HOST_DESKTOP_SESSION_PROXY_H
#define HOST_DESKTOP_SESSION_PROXY_H

#include "base/macros_magic.h"
#include "base/memory/local_memory.h"
#include "host/desktop_session.h"

namespace host {

class DesktopSessionManager;

class DesktopSessionProxy : public base::enable_shared_from_this<DesktopSessionProxy>
{
public:
    DesktopSessionProxy();
    ~DesktopSessionProxy();

    void control(proto::internal::DesktopControl::Action action);
    void configure(const DesktopSession::Config& config);
    void selectScreen(const proto::Screen& screen);
    void captureScreen();
    void setScreenCaptureFps(int fps);
    int screenCaptureFps() const;
    int defaultScreenCaptureFps() const;
    int minScreenCaptureFps() const;
    int maxScreenCaptureFps() const;
    void injectKeyEvent(const proto::KeyEvent& event);
    void injectTextEvent(const proto::TextEvent& event);
    void injectMouseEvent(const proto::MouseEvent& event);
    void injectClipboardEvent(const proto::ClipboardEvent& event);

    bool isMouseLocked() const { return is_mouse_locked_; }
    void setMouseLock(bool enable);

    bool isKeyboardLocked() const { return is_keyboard_locked_; }
    void setKeyboardLock(bool enable);

    bool isPaused() const { return is_paused_; }
    void setPaused(bool enable);

private:
    friend class DesktopSessionManager;

    void attachAndStart(DesktopSession* desktop_session);
    void stopAndDettach();

    DesktopSession* desktop_session_ = nullptr;

    bool is_mouse_locked_ = false;
    bool is_keyboard_locked_ = false;
    bool is_paused_ = false;

    static const int kDefaultScreenCaptureFps = 20;
    static const int kMinScreenCaptureFps = 1;
    static const int kMaxScreenCaptureFps = 30;

    int screen_capture_fps_ = kDefaultScreenCaptureFps;
    int default_capture_fps_ = kDefaultScreenCaptureFps;
    int min_capture_fps_ = kMinScreenCaptureFps;
    int max_capture_fps_ = kMaxScreenCaptureFps;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionProxy);
};

} // namespace host

#endif // HOST_DESKTOP_SESSION_PROXY_H
