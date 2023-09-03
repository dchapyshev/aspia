//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_DESKTOP_WINDOW_H
#define CLIENT_DESKTOP_WINDOW_H

#include "proto/desktop.pb.h"
#include "proto/desktop_extensions.pb.h"

#include <chrono>
#include <memory>
#include <string>

namespace base {
class Frame;
class MouseCursor;
class Size;
class Version;
} // namespace base

namespace proto {
namespace system_info {
class SystemInfo;
} // namespace system_info

namespace task_manager {
class HostToClient;
} // namespace task_manager

} // namespace proto

namespace client {

class DesktopControlProxy;
class FrameFactory;

class DesktopWindow
{
public:
    virtual ~DesktopWindow() = default;

    struct Metrics
    {
        std::chrono::seconds duration;
        int64_t total_rx = 0;
        int64_t total_tx = 0;
        int speed_rx = 0;
        int speed_tx = 0;
        int64_t video_packet_count = 0;
        int64_t video_pause_count = 0;
        int64_t video_resume_count = 0;
        size_t min_video_packet = 0;
        size_t max_video_packet = 0;
        size_t avg_video_packet = 0;
        int64_t audio_packet_count = 0;
        int64_t audio_pause_count = 0;
        int64_t audio_resume_count = 0;
        size_t min_audio_packet = 0;
        size_t max_audio_packet = 0;
        size_t avg_audio_packet = 0;
        uint32_t video_capturer_type = 0;
        int fps = 0;
        int send_mouse = 0;
        int drop_mouse = 0;
        int send_key = 0;
        int send_text = 0;
        int read_clipboard = 0;
        int send_clipboard = 0;
        int cursor_shape_count = 0;
        int cursor_pos_count = 0;
        int cursor_cached = 0;
        int cursor_taken_from_cache = 0;
    };

    virtual void showWindow(std::shared_ptr<DesktopControlProxy> desktop_control_proxy,
                            const base::Version& peer_version) = 0;

    virtual void configRequired() = 0;

    virtual void setCapabilities(const proto::DesktopCapabilities& capabilities) = 0;
    virtual void setScreenList(const proto::ScreenList& screen_list) = 0;
    virtual void setCursorPosition(const proto::CursorPosition& cursor_position) = 0;
    virtual void setSystemInfo(const proto::system_info::SystemInfo& system_info) = 0;
    virtual void setTaskManager(const proto::task_manager::HostToClient& message) = 0;
    virtual void setMetrics(const Metrics& metrics) = 0;

    virtual std::unique_ptr<FrameFactory> frameFactory() = 0;
    virtual void setFrameError(proto::VideoErrorCode error_code) = 0;
    virtual void setFrame(const base::Size& screen_size,
                          std::shared_ptr<base::Frame> frame) = 0;
    virtual void drawFrame() = 0;
    virtual void setMouseCursor(std::shared_ptr<base::MouseCursor> mouse_cursor) = 0;
};

} // namespace client

#endif // CLIENT_DESKTOP_WINDOW_H
