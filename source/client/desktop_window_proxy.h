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

#ifndef CLIENT__DESKTOP_WINDOW_PROXY_H
#define CLIENT__DESKTOP_WINDOW_PROXY_H

#include "base/macros_magic.h"

#include <memory>
#include <string>

namespace base {
class TaskRunner;
class Version;
} // namespace base

namespace desktop {
class Frame;
class MouseCursor;
class Size;
} // namespace desktop

namespace proto {
class ClipboardEvent;
class DesktopConfig;
class ScreenList;
class SystemInfo;
} // namespace proto

namespace client {

class DesktopControlProxy;
class DesktopWindow;

class DesktopWindowProxy
{
public:
    ~DesktopWindowProxy();

    static std::unique_ptr<DesktopWindowProxy> create(
        std::shared_ptr<base::TaskRunner> ui_task_runner, DesktopWindow* desktop_window);

    void showWindow(std::shared_ptr<DesktopControlProxy> desktop_control_proxy,
                    const base::Version& peer_version);

    void configRequired();

    void setCapabilities(const std::string& extensions, uint32_t video_encodings);
    void setScreenList(const proto::ScreenList& screen_list);
    void setSystemInfo(const proto::SystemInfo& system_info);

    std::shared_ptr<desktop::Frame> allocateFrame(const desktop::Size& size);
    void drawFrame(std::shared_ptr<desktop::Frame> frame);
    void drawMouseCursor(std::shared_ptr<desktop::MouseCursor> mouse_cursor);

    void injectClipboardEvent(const proto::ClipboardEvent& event);

private:
    DesktopWindowProxy(std::shared_ptr<base::TaskRunner> ui_task_runner,
                       DesktopWindow* desktop_window);

    class Impl;
    std::shared_ptr<Impl> impl_;

    DISALLOW_COPY_AND_ASSIGN(DesktopWindowProxy);
};

} // namespace client

#endif // CLIENT__DESKTOP_WINDOW_PROXY_H
