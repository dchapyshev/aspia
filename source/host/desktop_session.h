//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_DESKTOP_SESSION_H
#define HOST_DESKTOP_SESSION_H

#include <QObject>

#include "base/desktop/frame.h"
#include "base/desktop/mouse_cursor.h"
#include "proto/desktop_internal.h"

namespace host {

class DesktopSession : public QObject
{
    Q_OBJECT

public:
    explicit DesktopSession(QObject* parent);
    virtual ~DesktopSession() = default;

    struct Config
    {
        bool disable_font_smoothing = true;
        bool disable_wallpaper = true;
        bool disable_effects = true;
        bool block_input = false;
        bool lock_at_disconnect = false;
        bool clear_clipboard = true;
        bool cursor_position = false;

        bool equals(const Config& other) const
        {
            return (disable_font_smoothing == other.disable_font_smoothing) &&
                   (disable_wallpaper == other.disable_wallpaper) &&
                   (disable_effects == other.disable_effects) &&
                   (block_input == other.block_input) &&
                   (lock_at_disconnect == other.lock_at_disconnect) &&
                   (clear_clipboard == other.clear_clipboard) &&
                   (cursor_position == other.cursor_position);
        }
    };

    virtual void start() = 0;

    virtual void control(proto::internal::DesktopControl::Action action) = 0;
    virtual void configure(const Config& config) = 0;
    virtual void selectScreen(const proto::desktop::Screen& screen) = 0;
    virtual void captureScreen() = 0;

    virtual void setScreenCaptureFps(int fps) = 0;

    virtual void injectKeyEvent(const proto::desktop::KeyEvent& event) = 0;
    virtual void injectTextEvent(const proto::desktop::TextEvent& event) = 0;
    virtual void injectMouseEvent(const proto::desktop::MouseEvent& event) = 0;
    virtual void injectTouchEvent(const proto::desktop::TouchEvent& event) = 0;
    virtual void injectClipboardEvent(const proto::desktop::ClipboardEvent& event) = 0;

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
};

} // namespace host

#endif // HOST_DESKTOP_SESSION_H
