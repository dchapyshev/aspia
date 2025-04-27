//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/desktop/geometry.h"
#include "base/desktop/mouse_cursor.h"
#include "client/client.h"
#include "client/input_event_filter.h"
#include "common/clipboard_monitor.h"
#include "proto/desktop.h"
#include "proto/desktop_extensions.h"
#include "proto/system_info.h"
#include "proto/task_manager.h"

#include <filesystem>

namespace base {
class AudioDecoder;
class AudioPlayer;
class CursorDecoder;
class Frame;
class VideoDecoder;
class WebmFileWriter;
class WebmVideoEncoder;
} // namespace base

namespace client {

class AudioRenderer;

class ClientDesktop final : public Client
{
    Q_OBJECT

public:
    explicit ClientDesktop(QObject* parent = nullptr);
    ~ClientDesktop() final;

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

public slots:
    void setDesktopConfig(const proto::DesktopConfig& config);
    void setCurrentScreen(const proto::Screen& screen);
    void setPreferredSize(int width, int height);
    void setVideoPause(bool enable);
    void setAudioPause(bool enable);
    void setVideoRecording(bool enable, const std::filesystem::path& file_path);
    void onKeyEvent(const proto::KeyEvent& event);
    void onTextEvent(const proto::TextEvent& event);
    void onMouseEvent(const proto::MouseEvent& event);
    void onPowerControl(proto::PowerControl::Action action);
    void onRemoteUpdate();
    void onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request);
    void onTaskManager(const proto::task_manager::ClientToHost& message);
    void onMetricsRequest();

signals:
    void sig_showWindow();
    void sig_configRequired();
    void sig_capabilities(const proto::DesktopCapabilities& capabilities);
    void sig_screenListChanged(const proto::ScreenList& screen_list);
    void sig_screenTypeChanged(const proto::ScreenType& screen_type);
    void sig_cursorPositionChanged(const proto::CursorPosition& cursor_position);
    void sig_systemInfo(const proto::system_info::SystemInfo& system_info);
    void sig_taskManager(const proto::task_manager::HostToClient& message);
    void sig_metrics(const client::ClientDesktop::Metrics& metrics);
    void sig_frameError(proto::VideoErrorCode error_code);
    void sig_frameChanged(const base::Size& screen_size, std::shared_ptr<base::Frame> frame);
    void sig_drawFrame();
    void sig_mouseCursorChanged(std::shared_ptr<base::MouseCursor> mouse_cursor);

protected:
    // Client implementation.
    void onSessionStarted() final;
    void onSessionMessageReceived(uint8_t channel_id, const QByteArray& buffer) final;
    void onSessionMessageWritten(uint8_t channel_id, size_t pending) final;

private slots:
    void onClipboardEvent(const proto::ClipboardEvent& event);

private:
    void readCapabilities(const proto::DesktopCapabilities& capabilities);
    void readVideoPacket(const proto::VideoPacket& packet);
    void readAudioPacket(const proto::AudioPacket& packet);
    void readCursorShape(const proto::CursorShape& cursor_shape);
    void readCursorPosition(const proto::CursorPosition& cursor_position);
    void readClipboardEvent(const proto::ClipboardEvent& event);
    void readExtension(const proto::DesktopExtension& extension);

    bool started_ = false;

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

    std::unique_ptr<QTimer> webm_video_encode_timer_;
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

Q_DECLARE_METATYPE(client::ClientDesktop::Metrics)

#endif // CLIENT_CLIENT_DESKTOP_H
