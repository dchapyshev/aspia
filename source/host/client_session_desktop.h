//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__CLIENT_SESSION_DESKTOP_H
#define HOST__CLIENT_SESSION_DESKTOP_H

#include "base/macros_magic.h"
#include "host/client_session.h"

namespace host {

class ClipboardMonitor;
class InputInjector;
class MouseCursorMonitor;
class ScreenControls;
class VideoCapturer;

class ClientSessionDesktop : public ClientSession
{
public:
    ClientSessionDesktop(proto::SessionType session_type, std::unique_ptr<net::Channel> channel);
    ~ClientSessionDesktop();

    void setClipboardMonitor(std::unique_ptr<ClipboardMonitor> clipboard_monitor);
    void setInputInjector(std::unique_ptr<InputInjector> input_injector);
    void setMouseCursorMonitor(std::unique_ptr<MouseCursorMonitor> mouse_cursor_monitor);
    void setScreenControls(std::unique_ptr<ScreenControls> screen_controls);
    void setVideoCapturer(std::unique_ptr<VideoCapturer> video_capturer);

protected:
    // net::Listener implementation.
    void onNetworkMessage(base::ByteArray& buffer) override;

private:
    std::unique_ptr<ClipboardMonitor> clipboard_monitor_;
    std::unique_ptr<InputInjector> input_injector_;
    std::unique_ptr<MouseCursorMonitor> mouse_cursor_monitor_;
    std::unique_ptr<ScreenControls> screen_controls_;
    std::unique_ptr<VideoCapturer> video_capturer_;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionDesktop);
};

} // namespace host

#endif // HOST__CLIENT_SESSION_DESKTOP_H
