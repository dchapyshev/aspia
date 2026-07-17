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

#include <memory>

#include "base/codec/yuv_converter.h"
#include "base/desktop/shared_frame.h"
#include "base/threading/worker.h"
#include "proto/desktop_video.h"

class QTimer;
class VideoDecoder;
class WebmVideoEncoder;

class VideoWorker final : public Worker
{
    Q_OBJECT

public:
    VideoWorker();
    ~VideoWorker() final;

public slots:
    // Decodes one video packet and converts it for rendering. Runs in the worker thread; invoke it
    // through a queued connection.
    void onVideoPacket(std::shared_ptr<proto::video::Packet> packet);

    // Starts or stops encoding of decoded frames for the session recording.
    void onSetRecording(bool enable);

signals:
    void sig_frameError(proto::video::ErrorCode error_code);
    void sig_frameChanged(const QSize& screen_size, SharedFrame frame);
    // One packet decoded and converted; |packet_size| is the encoded packet size for statistics.
    void sig_drawFrame(const QList<QRect>& dirty_rects, size_t packet_size);
    void sig_videoInfoChanged(quint32 capturer_type, quint32 encoder_type, bool hardware_decoder);
    void sig_keyFrameRequired();
    void sig_temporaryError();
    void sig_h264Disabled();
    void sig_recordingVideoPacket(std::shared_ptr<proto::video::Packet> packet);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private:
    std::unique_ptr<VideoDecoder> decoder_;
    proto::video::Encoding encoding_ = proto::video::ENCODING_UNKNOWN;
    bool key_frame_received_ = false;

    YuvConverter yuv_converter_;
    QSize screen_size_;
    QList<QRect> dirty_rects_;
    quint32 capturer_type_ = 0;

    QTimer* recording_timer_ = nullptr;
    std::unique_ptr<WebmVideoEncoder> recording_encoder_;

    // Set once a hardware H264 decoder reports a permanent failure; sticks for the rest of the
    // session so the software backend is picked on every subsequent VideoDecoder::create() call.
    bool h264_hw_enabled_ = true;
    // Set after both HW and SW H264 decoders failed permanently (e.g. resolution exceeds H264
    // level limits). The client drops H264 from its capabilities so the host switches to VP.
    bool h264_sw_enabled_ = true;

    Q_DISABLE_COPY_MOVE(VideoWorker)
};

#endif // CLIENT_WORKERS_VIDEO_WORKER_H
