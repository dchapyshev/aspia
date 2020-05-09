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

#ifndef HOST__DESKTOP_SESSION_PROXY_H
#define HOST__DESKTOP_SESSION_PROXY_H

#include "base/macros_magic.h"
#include "host/desktop_session.h"

namespace host {

class DesktopSessionManager;

class DesktopSessionProxy : public std::enable_shared_from_this<DesktopSessionProxy>
{
public:
    DesktopSessionProxy();
    ~DesktopSessionProxy();

    void control(proto::internal::Control::Action action);
    void configure(const DesktopSession::Config& config);
    void selectScreen(const proto::Screen& screen);
    void captureScreen();
    void injectKeyEvent(const proto::KeyEvent& event);
    void injectPointerEvent(const proto::PointerEvent& event);
    void injectClipboardEvent(const proto::ClipboardEvent& event);

private:
    friend class DesktopSessionManager;

    void attachAndStart(DesktopSession* desktop_session);
    void stopAndDettach();

    DesktopSession* desktop_session_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionProxy);
};

} // namespace host

#endif // HOST__DESKTOP_SESSION_PROXY_H
