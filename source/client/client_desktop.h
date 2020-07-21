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

#ifndef CLIENT__CLIENT_DESKTOP_H
#define CLIENT__CLIENT_DESKTOP_H

#include "base/macros_magic.h"
#include "base/desktop/geometry.h"
#include "client/client.h"
#include "client/desktop_control.h"
#include "client/input_event_filter.h"
#include "proto/system_info.pb.h"

namespace base {
class Frame;
} // namespace base

namespace codec {
class CursorDecoder;
class VideoDecoder;
} // namespace codec

namespace client {

class DesktopControlProxy;
class DesktopWindow;
class DesktopWindowProxy;

class ClientDesktop
    : public Client,
      public DesktopControl
{
public:
    explicit ClientDesktop(std::shared_ptr<base::TaskRunner> io_task_runner);
    ~ClientDesktop();

    void setDesktopWindow(std::shared_ptr<DesktopWindowProxy> desktop_window_proxy);

    // DesktopControl implementation.
    void setDesktopConfig(const proto::DesktopConfig& config) override;
    void setCurrentScreen(const proto::Screen& screen) override;
    void setPreferredSize(int width, int height) override;
    void onKeyEvents(const std::vector<proto::KeyEvent>& events) override;
    void onMouseEvents(const std::vector<proto::MouseEvent>& events) override;
    void onClipboardEvent(const proto::ClipboardEvent& event) override;
    void onPowerControl(proto::PowerControl::Action action) override;
    void onRemoteUpdate() override;
    void onSystemInfoRequest() override;
    void onMetricsRequest() override;

protected:
    // Client implementation.
    void onSessionStarted(const base::Version& peer_version) override;

    // net::Channel::Listener implementation.
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten(size_t pending) override;

private:
    void readConfigRequest(const proto::DesktopConfigRequest& config_request);
    void readVideoPacket(const proto::VideoPacket& packet);
    void readCursorShape(const proto::CursorShape& cursor_shape);
    void readClipboardEvent(const proto::ClipboardEvent& event);
    void readExtension(const proto::DesktopExtension& extension);

    bool started_ = false;

    std::shared_ptr<DesktopControlProxy> desktop_control_proxy_;
    std::shared_ptr<DesktopWindowProxy> desktop_window_proxy_;
    std::shared_ptr<base::Frame> desktop_frame_;
    proto::DesktopConfig desktop_config_;

    proto::HostToClient incoming_message_;
    proto::ClientToHost outgoing_message_;

    proto::VideoEncoding video_encoding_ = proto::VIDEO_ENCODING_UNKNOWN;
    std::unique_ptr<codec::VideoDecoder> video_decoder_;
    std::unique_ptr<codec::CursorDecoder> cursor_decoder_;

    InputEventFilter input_event_filter_;

    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint start_time_;
    TimePoint begin_time_;
    int64_t video_frame_count_ = 0;
    size_t min_video_packet_ = std::numeric_limits<size_t>::max();
    size_t max_video_packet_ = 0;
    size_t avg_video_packet_ = 0;
    int fps_ = 0;

    DISALLOW_COPY_AND_ASSIGN(ClientDesktop);
};

} // namespace client

#endif // CLIENT__CLIENT_DESKTOP_H
