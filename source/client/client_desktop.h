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

#include <QList>
#include <QPointer>
#include <QRect>
#include <QSize>

#include "base/serialization.h"
#include "base/desktop/shared_frame.h"
#include "client/client.h"
#include "common/clipboard.h"
#include "common/clipboard_file_transfer.h"
#include "proto/desktop_audio.h"
#include "proto/desktop_control.h"
#include "proto/desktop_cursor.h"
#include "proto/desktop_input.h"
#include "proto/desktop_power.h"
#include "proto/desktop_screen.h"
#include "proto/desktop_video.h"

namespace proto::clipboard {
class Event;
} // namespace proto::clipboard

namespace proto::cursor {
class Position;
class Shape;
} // namespace proto::cursor

namespace proto::input {
class KeyEvent;
class MouseEvent;
class TextEvent;
} // namespace proto::input

namespace proto::legacy {
class Capabilities;
class Extension;
} // namespace proto::legacy

namespace proto::screen {
class Screen;
class ScreenList;
class ScreenType;
} // namespace proto::screen

namespace proto::task_manager {
class ClientToHost;
class HostToClient;
} // namespace proto::task_manager

class AudioWorker;
class CursorDecoder;
class MouseCursor;
class QTimer;
class VideoWorker;
class WebmFileWriter;
class WorkerManager;

class ClientDesktop final : public Client
{
    Q_OBJECT

public:
    explicit ClientDesktop(const proto::control::Config& desktop_config, QObject* parent = nullptr);
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
        UdpMethod udp_method {};
        qint64 video_packet_count = 0;
        size_t min_video_packet = 0;
        size_t max_video_packet = 0;
        size_t avg_video_packet = 0;
        qint64 audio_packet_count = 0;
        size_t min_audio_packet = 0;
        size_t max_audio_packet = 0;
        size_t avg_audio_packet = 0;
        quint32 video_capturer_type = 0;
        quint32 video_encoder_type = 0;
        bool video_decoder_hardware = false;
        int fps = 0;
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
    void onRecordingChanged(bool enable, const QString& file_path);
    void onKeyEvent(const proto::input::KeyEvent& event);
    void onTextEvent(const proto::input::TextEvent& event);
    void onMouseEvent(const proto::input::MouseEvent& event);
    void onPowerControl(proto::power::Control_Action action);
    void onTaskManager(const proto::task_manager::ClientToHost& message);
    void onMetricsRequest();
    void onSwitchSession(quint32 session_id);
    void onClipboardEvent(const proto::clipboard::Event& event);
    void onClipboardLocalFileListChanged(const QVector<LocalFileEntry>& files);
    void onClipboardFileDataRequest(int file_index);

signals:
    void sig_capabilities(const proto::control::Capabilities& capabilities);
    void sig_injectClipboardEvent(const proto::clipboard::Event& event);
    void sig_clipboardFileData(int file_index, const QByteArray& data, bool is_last);
    void sig_screenListChanged(const proto::screen::ScreenList& screen_list);
    void sig_screenTypeChanged(const proto::screen::ScreenType& screen_type);
    void sig_cursorPositionChanged(const proto::cursor::Position& position);
    void sig_taskManager(const proto::task_manager::HostToClient& message);
    void sig_metrics(const ClientDesktop::Metrics& metrics);
    void sig_frameError(proto::video::ErrorCode error_code);
    void sig_frameChanged(const QSize& screen_size, SharedFrame frame);
    void sig_drawFrame(const QList<QRect>& dirty_rects);
    void sig_mouseCursorChanged(std::shared_ptr<MouseCursor> mouse_cursor);
    void sig_sessionListChanged(const proto::control::SessionList& sessions);
    void sig_audioPacket(std::shared_ptr<proto::audio::Packet> packet);
    void sig_videoPacket(std::shared_ptr<proto::video::Packet> packet);
    void sig_videoRecording(bool enable);

protected:
    // Client implementation.
    void onStarted() final;
    void onMessageReceived(quint8 channel_id, const QByteArray& buffer) final;

private slots:
    void onRepeatedTimer();
    void onVideoDrawFrame(const QList<QRect>& dirty_rects, size_t packet_size);
    void onVideoInfoChanged(quint32 capturer_type, quint32 encoder_type, bool hardware_decoder);
    void onVideoKeyFrameRequired();
    void onVideoTemporaryError();
    void onVideoH264Disabled();
    void onRecordingVideoPacket(std::shared_ptr<proto::video::Packet> packet);

private:
    void readLegacyCapabilities(const proto::legacy::Capabilities& capabilities);
    void readCapabilities(const proto::control::Capabilities& capabilities);
    void readVideoPacket(const proto::video::Packet& packet);
    void readAudioPacket(const proto::audio::Packet& packet);
    void readCursorShape(const proto::cursor::Shape& shape);
    void readCursorPosition(const proto::cursor::Position& position);
    void readClipboardEvent(const proto::clipboard::Event& event);
    void readExtension(const proto::legacy::Extension& extension);
    void sendSessionListRequest();
    void sendConfig(const proto::control::Config& config);
    void sendKeyFrameRequest();
    void sendCapabilities();
    void setForceReliable(bool enable);

    QTimer* repeated_timer_ = nullptr;

    bool started_ = false;

    proto::control::Config desktop_config_;

    Parser<proto::video::HostToClient,
           proto::cursor::HostToClient,
           proto::screen::HostToClient,
           proto::audio::HostToClient> incoming_message_;

    Serializer<proto::input::ClientToHost> outgoing_message_;

    std::unique_ptr<CursorDecoder> cursor_decoder_;
    ClipboardFileTransfer* clipboard_file_transfer_ = nullptr;

    std::unique_ptr<WorkerManager> worker_manager_;
    QPointer<AudioWorker> audio_worker_;
    QPointer<VideoWorker> video_worker_;

    std::unique_ptr<WebmFileWriter> webm_file_writer_;

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    qint64 video_packet_count_ = 0;
    qint64 audio_packet_count_ = 0;
    int read_clipboard_count_ = 0;
    int send_clipboard_count_ = 0;
    quint32 video_capturer_type_ = 0;
    quint32 video_encoder_type_ = 0;
    bool video_decoder_hardware_ = false;
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

    bool file_clipboard_supported_ = false;
    bool force_reliable_active_ = false;
    int reliable_hold_seconds_ = 0;
    int reliable_disable_count_ = 0;

    // Cleared when the video worker reports a permanent H264 failure. sendCapabilities() drops
    // kFlagVideoH264 so the host switches to VP.
    bool h264_sw_enabled_ = true;

    Q_DISABLE_COPY_MOVE(ClientDesktop)
};

Q_DECLARE_METATYPE(ClientDesktop::Metrics)

#endif // CLIENT_CLIENT_DESKTOP_H
