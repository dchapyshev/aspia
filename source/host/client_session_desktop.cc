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

#include "host/client_session_desktop.h"

#include "host/clipboard_monitor.h"
#include "host/input_injector.h"
#include "host/mouse_cursor_monitor.h"
#include "host/screen_controls.h"
#include "host/video_capturer.h"
#include "net/network_channel.h"

namespace host {

ClientSessionDesktop::ClientSessionDesktop(
    proto::SessionType session_type, std::unique_ptr<net::Channel> channel)
    : ClientSession(session_type, std::move(channel))
{
    // Nothing
}

ClientSessionDesktop::~ClientSessionDesktop() = default;

void ClientSessionDesktop::setClipboardMonitor(std::unique_ptr<ClipboardMonitor> clipboard_monitor)
{
    clipboard_monitor_ = std::move(clipboard_monitor);
}

void ClientSessionDesktop::setInputInjector(std::unique_ptr<InputInjector> input_injector)
{
    input_injector_ = std::move(input_injector);
}

void ClientSessionDesktop::setMouseCursorMonitor(
    std::unique_ptr<MouseCursorMonitor> mouse_cursor_monitor)
{
    mouse_cursor_monitor_ = std::move(mouse_cursor_monitor);
}

void ClientSessionDesktop::setScreenControls(std::unique_ptr<ScreenControls> screen_controls)
{
    screen_controls_ = std::move(screen_controls);
}

void ClientSessionDesktop::setVideoCapturer(std::unique_ptr<VideoCapturer> video_capturer)
{
    video_capturer_ = std::move(video_capturer);
}

void ClientSessionDesktop::onMessageReceived(const base::ByteArray& buffer)
{
    // TODO
}

void ClientSessionDesktop::onMessageWritten()
{

}

} // namespace host
