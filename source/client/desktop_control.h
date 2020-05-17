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

#ifndef CLIENT__DESKTOP_CONTROL_H
#define CLIENT__DESKTOP_CONTROL_H

#include "proto/desktop.pb.h"
#include "proto/desktop_extensions.pb.h"

namespace client {

class DesktopControl
{
public:
    virtual ~DesktopControl() = default;

    virtual void setDesktopConfig(const proto::DesktopConfig& desktop_config) = 0;
    virtual void setCurrentScreen(const proto::Screen& screen) = 0;
    virtual void setPreferredSize(int width, int height) = 0;

    virtual void onKeyEvent(const proto::KeyEvent& event) = 0;
    virtual void onMouseEvents(const std::vector<proto::MouseEvent>& event) = 0;
    virtual void onClipboardEvent(const proto::ClipboardEvent& event) = 0;
    virtual void onPowerControl(proto::PowerControl::Action action) = 0;
    virtual void onRemoteUpdate() = 0;
    virtual void onSystemInfoRequest() = 0;
    virtual void onMetricsRequest() = 0;
};

} // namespace client

#endif // CLIENT__DESKTOP_CONTROL_H
