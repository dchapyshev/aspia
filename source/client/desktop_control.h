//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_DESKTOP_CONTROL_H
#define CLIENT_DESKTOP_CONTROL_H

#include "proto/desktop.pb.h"
#include "proto/desktop_extensions.pb.h"
#include "proto/system_info.pb.h"
#include "proto/task_manager.pb.h"

#include <filesystem>

namespace client {

class DesktopControl
{
public:
    virtual ~DesktopControl() = default;

    virtual void setDesktopConfig(const proto::DesktopConfig& desktop_config) = 0;
    virtual void setCurrentScreen(const proto::Screen& screen) = 0;
    virtual void setPreferredSize(int width, int height) = 0;
    virtual void setVideoPause(bool enable) = 0;
    virtual void setAudioPause(bool enable) = 0;
    virtual void setVideoRecording(bool enable, const std::filesystem::path& file_path) = 0;

    virtual void onKeyEvent(const proto::KeyEvent& event) = 0;
    virtual void onTextEvent(const proto::TextEvent& event) = 0;
    virtual void onMouseEvent(const proto::MouseEvent& event) = 0;
    virtual void onPowerControl(proto::PowerControl::Action action) = 0;
    virtual void onRemoteUpdate() = 0;
    virtual void onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request) = 0;
    virtual void onTaskManager(const proto::task_manager::ClientToHost& message) = 0;
    virtual void onMetricsRequest() = 0;
};

} // namespace client

#endif // CLIENT_DESKTOP_CONTROL_H
