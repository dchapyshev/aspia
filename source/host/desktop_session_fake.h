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

#ifndef HOST_DESKTOP_SESSION_FAKE_H
#define HOST_DESKTOP_SESSION_FAKE_H

#include "host/desktop_session.h"

namespace host {

class DesktopSessionFake final : public DesktopSession
{
    Q_OBJECT

public:
    explicit DesktopSessionFake(QObject* parent = nullptr);
    ~DesktopSessionFake() final;

    // DesktopSession implementation.
    void start() final;
    void control(proto::internal::DesktopControl::Action action) final;
    void configure(const Config& config) final;
    void selectScreen(const proto::desktop::Screen& screen) final;
    void captureScreen() final;
    void setScreenCaptureFps(int fps) final;
    void injectKeyEvent(const proto::desktop::KeyEvent& event) final;
    void injectTextEvent(const proto::desktop::TextEvent& event) final;
    void injectMouseEvent(const proto::desktop::MouseEvent& event) final;
    void injectTouchEvent(const proto::desktop::TouchEvent& event) final;
    void injectClipboardEvent(const proto::desktop::ClipboardEvent& event) final;

private:
    Q_DISABLE_COPY(DesktopSessionFake)
};

} // namespace host

#endif // HOST_DESKTOP_SESSION_FAKE_H
