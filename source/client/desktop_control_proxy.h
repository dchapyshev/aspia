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

#ifndef CLIENT__DESKTOP_CONTROL_PROXY_H
#define CLIENT__DESKTOP_CONTROL_PROXY_H

#include "base/macros_magic.h"
#include "proto/desktop.pb.h"
#include "proto/desktop_extensions.pb.h"

namespace base {
class TaskRunner;
} // namespace base

namespace client {

class DesktopControl;

class DesktopControlProxy : public std::enable_shared_from_this<DesktopControlProxy>
{
public:
    DesktopControlProxy(std::shared_ptr<base::TaskRunner> io_task_runner,
                        DesktopControl* desktop_control);
    ~DesktopControlProxy();

    void dettach();

    void setDesktopConfig(const proto::DesktopConfig& desktop_config);
    void setCurrentScreen(const proto::Screen& screen);
    void onKeyEvent(const proto::KeyEvent& event);
    void onPointerEvent(const proto::PointerEvent& event);
    void onClipboardEvent(const proto::ClipboardEvent& event);
    void onPowerControl(proto::PowerControl::Action action);
    void onRemoteUpdate();
    void onSystemInfoRequest();

private:
    std::shared_ptr<base::TaskRunner> io_task_runner_;
    DesktopControl* desktop_control_;

    DISALLOW_COPY_AND_ASSIGN(DesktopControlProxy);
};

} // namespace client

#endif // CLIENT__DESKTOP_CONTROL_PROXY_H
