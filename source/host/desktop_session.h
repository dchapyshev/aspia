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

#ifndef HOST__DESKTOP_SESSION_H
#define HOST__DESKTOP_SESSION_H

#include <memory>

namespace desktop {
class Frame;
class MouseCursor;
} // namespace desktop

namespace proto {
class ClipboardEvent;
class KeyEvent;
class PointerEvent;
class Screen;
class ScreenList;
namespace internal {
class SetFeatures;
} // namespace internal
} // namespace proto

namespace host {

class DesktopSession
{
public:
    virtual ~DesktopSession() = default;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onDesktopSessionStarted() = 0;
        virtual void onDesktopSessionStopped() = 0;
        virtual void onScreenCaptured(const desktop::Frame& frame) = 0;
        virtual void onCursorCaptured(std::shared_ptr<desktop::MouseCursor> mouse_cursor) = 0;
        virtual void onScreenListChanged(const proto::ScreenList& list) = 0;
        virtual void onClipboardEvent(const proto::ClipboardEvent& event) = 0;
    };

    virtual void start() = 0;

    virtual void startSession() = 0;
    virtual void stopSession() = 0;
    virtual void selectScreen(const proto::Screen& screen) = 0;
    virtual void setFeatures(const proto::internal::SetFeatures& features) = 0;

    virtual void injectKeyEvent(const proto::KeyEvent& event) = 0;
    virtual void injectPointerEvent(const proto::PointerEvent& event) = 0;
    virtual void injectClipboardEvent(const proto::ClipboardEvent& event) = 0;

    virtual void logoffUserSession() = 0;
    virtual void lockUserSession() = 0;
};

} // namespace host

#endif // HOST__DESKTOP_SESSION_H
