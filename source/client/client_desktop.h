//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/serialization.h"
#include "base/desktop/mouse_cursor.h"
#include "client/client.h"
#include "client/input_event_filter.h"
#include "common/clipboard_monitor.h"
#include "proto/desktop_legacy.h"
#include "proto/desktop_control.h"
#include "proto/desktop_cursor.h"
#include "proto/desktop_power.h"
#include "proto/system_info.h"
#include "proto/task_manager.h"

class QTimer;

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
        qint64 total_tcp_rx = 0;
        qint64 total_tcp_tx = 0;
        int speed_tcp_rx = 0;
        int speed_tcp_tx = 0;
        qint64 total_udp_rx = 0;
        qint64 total_udp_tx = 0;
        int speed_udp_rx = 0;
        int speed_udp_tx = 0;
        qint64 video_packet_count = 0;
        qint64 video_pause_count = 0;
        qint64 video_resume_count = 0;
        size_t min_video_packet = 0;
        size_t max_video_packet = 0;
        size_t avg_video_packet = 0;
        qint64 audio_packet_count = 0;
        qint64 audio_pause_count = 0;
        qint64 audio_resume_count = 0;
        size_t min_audio_packet = 0;
        size_t max_audio_packet = 0;
        size_t avg_audio_packet = 0;
        quint32 video_capturer_type = 0;
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
    void onDesktopConfigChanged(const proto::control::Config& config);
    void onCurrentScreenChanged(const proto::screen::Screen& screen);
    void onPreferredSizeChanged(int width, int height);
    void onVideoPauseChanged(bool enable);
    void onAudioPauseChanged(bool enable);
    void onRecordingChanged(bool enable, const QString& file_path);
    void onKeyEvent(const proto::input::KeyEvent& event);
    void onTextEvent(const proto::input::TextEvent& event);
    void onMouseEvent(const proto::input::MouseEvent& event);
    void onPowerControl(proto::power::Control_Action action);
    void onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request);
    void onTaskManager(const proto::task_manager::ClientToHost& message);
    void onMetricsRequest();
    void onSwitchSession(quint32 session_id);

signals:
    void sig_capabilities(const proto::control::Capabilities& capabilities);
    void sig_screenListChanged(const proto::screen::ScreenList& screen_list);
    void sig_screenTypeChanged(const proto::screen::ScreenType& screen_type);
    void sig_cursorPositionChanged(const proto::cursor::Position& position);
    void sig_systemInfo(const proto::system_info::SystemInfo& system_info);
    void sig_taskManager(const proto::task_manager::HostToClient& message);
    void sig_metrics(const client::ClientDesktop::Metrics& metrics);
    void sig_frameError(proto::video::ErrorCode error_code);
    void sig_frameChanged(const QSize& screen_size, std::shared_ptr<base::Frame> frame);
    void sig_drawFrame();
    void sig_mouseCursorChanged(std::shared_ptr<base::MouseCursor> mouse_cursor);
    void sig_sessionListChanged(const proto::control::SessionList& sessions);
    void sig_videoEncodingChanged(proto::video::Encoding encoding);

protected:
    // Client implementation.
    void onStarted() final;
    void onMessageReceived(quint8 channel_id, const QByteArray& buffer) final;

private slots:
    void onClipboardEvent(const proto::clipboard::Event& event);

private:
    void readCapabilities(const proto::control::Capabilities& capabilities);
    void readVideoPacket(const proto::video::Packet& packet);
    void readAudioPacket(const proto::audio::Packet& packet);
    void readCursorShape(const proto::cursor::Shape& shape);
    void readCursorPosition(const proto::cursor::Position& position);
    void readClipboardEvent(const proto::clipboard::Event& event);
    void readExtension(const proto::legacy::Extension& extension);
    void sendSessionListRequest();
    void sendKeyFrameRequest();

    bool started_ = false;
    bool key_frame_received_ = false;

    std::shared_ptr<base::Frame> desktop_frame_;
    proto::control::Config desktop_config_;

    proto::video::Encoding video_encoding_ = proto::video::ENCODING_UNKNOWN;
    proto::audio::Encoding audio_encoding_ = proto::audio::ENCODING_UNKNOWN;

    std::unique_ptr<base::VideoDecoder> video_decoder_;
    std::unique_ptr<base::CursorDecoder> cursor_decoder_;
    std::unique_ptr<base::AudioDecoder> audio_decoder_;
    std::unique_ptr<base::AudioPlayer> audio_player_;
    common::ClipboardMonitor* clipboard_monitor_ = nullptr;

    InputEventFilter input_event_filter_;

    QTimer* webm_video_encode_timer_ = nullptr;
    std::unique_ptr<base::WebmVideoEncoder> webm_video_encoder_;
    std::unique_ptr<base::WebmFileWriter> webm_file_writer_;

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    qint64 video_packet_count_ = 0;
    qint64 video_pause_count_ = 0;
    qint64 video_resume_count_ = 0;
    qint64 audio_packet_count_ = 0;
    qint64 audio_pause_count_ = 0;
    qint64 audio_resume_count_ = 0;
    quint32 video_capturer_type_ = 0;
    TimePoint start_time_;
    TimePoint fps_time_;
    qint64 fps_frame_count_ = 0;
    size_t min_video_packet_ = std::numeric_limits<size_t>::max();
    size_t max_video_packet_ = 0;
    size_t avg_video_packet_ = 0;
    size_t min_audio_packet_ = std::numeric_limits<size_t>::max();
    size_t max_audio_packet_ = 0;
    size_t avg_audio_packet_ = 0;
    int fps_ = 0;
    int cursor_shape_count_ = 0;
    int cursor_pos_count_ = 0;

    Q_DISABLE_COPY_MOVE(ClientDesktop)
};

} // namespace client

Q_DECLARE_METATYPE(client::ClientDesktop::Metrics)

#endif // CLIENT_CLIENT_DESKTOP_H
