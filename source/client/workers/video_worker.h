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

#ifndef CLIENT_WORKERS_VIDEO_WORKER_H
#define CLIENT_WORKERS_VIDEO_WORKER_H

#include <QList>
#include <QRect>
#include <QSize>

#include "base/serialization.h"
#include "base/codec/yuv_converter.h"
#include "base/desktop/shared_frame.h"
#include "base/threading/worker.h"
#include "proto/desktop_channel.h"
#include "proto/desktop_cursor.h"
#include "proto/desktop_video.h"

class CursorDecoder;
class MouseCursor;
class VideoDecoder;

class VideoWorker final : public Worker
{
    Q_OBJECT

public:
    VideoWorker();
    ~VideoWorker() final;

    struct Metrics
    {
        qint64 packet_count = 0;
        size_t min_packet = 0;
        size_t max_packet = 0;
        size_t avg_packet = 0;
        int fps = 0;
        quint32 capturer_type = 0;
        quint32 encoder_type = 0;
        bool hardware_decoder = false;
        int cursor_shape_count = 0;
        int cursor_pos_count = 0;
        int cursor_cached = 0;
        int cursor_taken_from_cache = 0;
    };

public slots:
    void onVideoMessage(const QByteArray& buffer);
    void onVideoPacket(std::shared_ptr<proto::video::Packet> packet);
    void onCursorMessage(const QByteArray& buffer);
    void onCursorConfig(bool shape_enabled, bool position_enabled);

signals:
    void sig_sendMessage(quint8 channel_id, const QByteArray& buffer);
    void sig_frameError(proto::video::ErrorCode error_code);
    void sig_frameChanged(const QSize& screen_size, SharedFrame frame);
    void sig_drawFrame(const QList<QRect>& dirty_rects);
    void sig_h264Disabled();
    void sig_mouseCursorChanged(std::shared_ptr<MouseCursor> mouse_cursor);
    void sig_cursorPositionChanged(const proto::cursor::Position& position);
    void sig_metrics(const VideoWorker::Metrics& metrics);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;
    void onTimer() final;

private:
    void decodePacket(const proto::video::Packet& packet);
    void sendKeyFrameRequest();
    // Transport adaptation: on a temporary decode failure (packet loss over UDP) ask the host to
    // switch to reliable transport for a hold window; the metrics timer winds it back off after the
    // window, at most twice per session.
    void enableForceReliable();
    void sendForceReliable(bool enable);
    void readCursorShape(const proto::cursor::Shape& shape);
    void readCursorPosition(const proto::cursor::Position& position);

    Parser<proto::video::HostToClient, proto::cursor::HostToClient> incoming_message_;

    std::unique_ptr<VideoDecoder> decoder_;
    proto::video::Encoding encoding_ = proto::video::ENCODING_UNKNOWN;
    bool key_frame_received_ = false;

    // Set from the packet source: NG packets arrive on the video channel (onVideoMessage), legacy
    // packets through the multiplexed channel (onVideoPacket). Keyframe requests are only sent in
    // the NG mode, as the legacy protocol has no such request.
    bool legacy_ = false;

    std::unique_ptr<CursorDecoder> cursor_decoder_;
    bool cursor_shape_enabled_ = false;
    bool cursor_position_enabled_ = false;

    YuvConverter yuv_converter_;
    QSize screen_size_;
    QList<QRect> dirty_rects_;

    // Statistics, emitted once a second through sig_metrics.
    Metrics metrics_;
    qint64 fps_frame_count_ = 0;
    TimePoint fps_time_;

    // Set once a hardware H264 decoder reports a permanent failure; sticks for the rest of the
    // session so the software backend is picked on every subsequent VideoDecoder::create() call.
    bool h264_hw_enabled_ = true;
    // Set after both HW and SW H264 decoders failed permanently (e.g. resolution exceeds H264
    // level limits). The client drops H264 from its capabilities so the host switches to VP.
    bool h264_sw_enabled_ = true;

    // Force-reliable transport state, driven by temporary decode failures. reliable_hold_seconds_
    // counts down on the metrics timer; the switch is applied at most twice per session.
    bool force_reliable_active_ = false;
    int reliable_disable_count_ = 0;
    int reliable_hold_seconds_ = 0;

    Q_DISABLE_COPY_MOVE(VideoWorker)
};

Q_DECLARE_METATYPE(VideoWorker::Metrics)

#endif // CLIENT_WORKERS_VIDEO_WORKER_H
