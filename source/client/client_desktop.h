//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_CLIENT_DESKTOP_H
#define CLIENT_CLIENT_DESKTOP_H

#include "base/macros_magic.h"
#include "client/client.h"
#include "client/desktop_control.h"
#include "client/input_event_filter.h"
#include "common/clipboard_monitor.h"

namespace base {
class AudioDecoder;
class AudioPlayer;
class CursorDecoder;
class Frame;
class VideoDecoder;
class WaitableTimer;
class WebmFileWriter;
class WebmVideoEncoder;
} // namespace base

namespace client {

class AudioRenderer;
class DesktopControlProxy;
class DesktopWindow;
class DesktopWindowProxy;

class ClientDesktop
    : public Client,
      public DesktopControl,
      public common::Clipboard::Delegate
{
public:
    explicit ClientDesktop(std::shared_ptr<base::TaskRunner> io_task_runner);
    ~ClientDesktop() override;

    void setDesktopWindow(std::shared_ptr<DesktopWindowProxy> desktop_window_proxy);

    // DesktopControl implementation.
    void setDesktopConfig(const proto::DesktopConfig& config) override;
    void setCurrentScreen(const proto::Screen& screen) override;
    void setPreferredSize(int width, int height) override;
    void setVideoPause(bool enable) override;
    void setAudioPause(bool enable) override;
    void setVideoRecording(bool enable, const std::filesystem::path& file_path) override;
    void onKeyEvent(const proto::KeyEvent& event) override;
    void onTextEvent(const proto::TextEvent& event) override;
    void onMouseEvent(const proto::MouseEvent& event) override;
    void onPowerControl(proto::PowerControl::Action action) override;
    void onRemoteUpdate() override;
    void onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request) override;
    void onTaskManager(const proto::task_manager::ClientToHost& message) override;
    void onMetricsRequest() override;

protected:
    // Client implementation.
    void onSessionStarted() override;
    void onSessionMessageReceived(uint8_t channel_id, const base::ByteArray& buffer) override;
    void onSessionMessageWritten(uint8_t channel_id, size_t pending) override;

    // common::Clipboard::Delegate implementation.
    void onClipboardEvent(const proto::ClipboardEvent& event) override;

private:
    void readCapabilities(const proto::DesktopCapabilities& capabilities);
    void readVideoPacket(const proto::VideoPacket& packet);
    void readAudioPacket(const proto::AudioPacket& packet);
    void readCursorShape(const proto::CursorShape& cursor_shape);
    void readCursorPosition(const proto::CursorPosition& cursor_position);
    void readClipboardEvent(const proto::ClipboardEvent& event);
    void readExtension(const proto::DesktopExtension& extension);

    bool started_ = false;

    std::shared_ptr<DesktopControlProxy> desktop_control_proxy_;
    std::shared_ptr<DesktopWindowProxy> desktop_window_proxy_;
    std::shared_ptr<base::Frame> desktop_frame_;
    proto::DesktopConfig desktop_config_;

    std::unique_ptr<proto::HostToClient> incoming_message_;
    std::unique_ptr<proto::ClientToHost> outgoing_message_;

    proto::VideoEncoding video_encoding_ = proto::VIDEO_ENCODING_UNKNOWN;
    proto::AudioEncoding audio_encoding_ = proto::AUDIO_ENCODING_UNKNOWN;

    std::unique_ptr<base::VideoDecoder> video_decoder_;
    std::unique_ptr<base::CursorDecoder> cursor_decoder_;
    std::unique_ptr<base::AudioDecoder> audio_decoder_;
    std::unique_ptr<base::AudioPlayer> audio_player_;
    std::unique_ptr<common::ClipboardMonitor> clipboard_monitor_;

    InputEventFilter input_event_filter_;

    std::unique_ptr<base::WaitableTimer> webm_video_encode_timer_;
    std::unique_ptr<base::WebmVideoEncoder> webm_video_encoder_;
    std::unique_ptr<base::WebmFileWriter> webm_file_writer_;

    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    int64_t video_packet_count_ = 0;
    int64_t video_pause_count_ = 0;
    int64_t video_resume_count_ = 0;
    int64_t audio_packet_count_ = 0;
    int64_t audio_pause_count_ = 0;
    int64_t audio_resume_count_ = 0;
    uint32_t video_capturer_type_ = 0;
    TimePoint start_time_;
    TimePoint fps_time_;
    int64_t fps_frame_count_ = 0;
    size_t min_video_packet_ = std::numeric_limits<size_t>::max();
    size_t max_video_packet_ = 0;
    size_t avg_video_packet_ = 0;
    size_t min_audio_packet_ = std::numeric_limits<size_t>::max();
    size_t max_audio_packet_ = 0;
    size_t avg_audio_packet_ = 0;
    int fps_ = 0;
    int cursor_shape_count_ = 0;
    int cursor_pos_count_ = 0;

    DISALLOW_COPY_AND_ASSIGN(ClientDesktop);
};

} // namespace client

#endif // CLIENT_CLIENT_DESKTOP_H
