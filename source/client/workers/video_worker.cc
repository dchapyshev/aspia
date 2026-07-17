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

#include "client/workers/video_worker.h"

#include <QTimer>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/codec/cursor_decoder.h"
#include "base/codec/video_decoder.h"
#include "base/desktop/mouse_cursor.h"
#include "base/threading/worker_manager.h"
#include "client/workers/network_worker.h"
#include "proto/desktop_control.h"

namespace {

//--------------------------------------------------------------------------------------------------
int calculateFps(int last_fps, const std::chrono::milliseconds& duration, qint64 count)
{
    static const double kAlpha = 0.1;
    const qint64 ms = duration.count();
    if (ms <= 0)
        return last_fps;
    return int((kAlpha * ((1000.0 / double(ms)) * double(count))) + ((1.0 - kAlpha) * double(last_fps)));
}

//--------------------------------------------------------------------------------------------------
size_t calculateAvgSize(size_t last_avg_size, size_t bytes)
{
    static const double kAlpha = 0.1;
    return static_cast<size_t>(
        (kAlpha * static_cast<double>(bytes)) +
        ((1.0 - kAlpha) * static_cast<double>(last_avg_size)));
}

} // namespace

//--------------------------------------------------------------------------------------------------
VideoWorker::VideoWorker()
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
VideoWorker::~VideoWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::onVideoMessage(const QByteArray& buffer)
{
    legacy_ = false;

    proto::video::HostToClient* message = incoming_message_.parse<proto::video::HostToClient>(buffer);
    if (!message)
    {
        LOG(ERROR) << "Unable to parse video message";
        return;
    }

    if (message->has_packet())
        decodePacket(message->packet());
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::onVideoPacket(std::shared_ptr<proto::video::Packet> packet)
{
    legacy_ = true;

    if (packet)
        decodePacket(*packet);
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::onCursorMessage(const QByteArray& buffer)
{
    proto::cursor::HostToClient* message =
        incoming_message_.parse<proto::cursor::HostToClient>(buffer);
    if (!message)
    {
        LOG(ERROR) << "Unable to parse cursor message";
        return;
    }

    if (message->has_shape())
        readCursorShape(message->shape());
    else if (message->has_position())
        readCursorPosition(message->position());
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::onCursorConfig(bool shape_enabled, bool position_enabled)
{
    cursor_shape_enabled_ = shape_enabled;
    cursor_position_enabled_ = position_enabled;

    if (!cursor_shape_enabled_)
    {
        LOG(INFO) << "Cursor shape disabled";
        cursor_decoder_.reset();
    }
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::onStart()
{
    LOG(INFO) << "Video worker started";

    NetworkWorker* network_worker = findWorker<NetworkWorker>();
    if (network_worker)
    {
        // Video and cursor channels (CHANNEL_ID_VIDEO == 3, CHANNEL_ID_CURSOR == 4).
        connect(network_worker, &NetworkWorker::sig_channel_3, this, &VideoWorker::onVideoMessage,
                Qt::QueuedConnection);
        connect(network_worker, &NetworkWorker::sig_channel_4, this, &VideoWorker::onCursorMessage,
                Qt::QueuedConnection);
        connect(this, &VideoWorker::sig_sendMessage, network_worker, &NetworkWorker::onSendMessage,
                Qt::QueuedConnection);
    }
    else
    {
        LOG(ERROR) << "Network worker not found";
    }

    metrics_timer_ = new QTimer(this);
    metrics_timer_->setInterval(std::chrono::seconds(1));
    connect(metrics_timer_, &QTimer::timeout, this, &VideoWorker::onMetricsTimer);
    metrics_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::onStop()
{
    LOG(INFO) << "Video worker stopped";

    delete metrics_timer_;
    metrics_timer_ = nullptr;

    decoder_.reset();
    cursor_decoder_.reset();
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::onMetricsTimer()
{
    // The 1-second timer also winds down the force-reliable hold window; once it elapses, back the
    // transport off reliable mode (at most twice per session).
    if (force_reliable_active_ && reliable_hold_seconds_ > 0)
    {
        --reliable_hold_seconds_;
        if (reliable_hold_seconds_ == 0 && reliable_disable_count_ < 2)
        {
            ++reliable_disable_count_;
            LOG(INFO) << "Disabling force reliable (disable count:" << reliable_disable_count_ << ")";
            force_reliable_active_ = false;
            sendForceReliable(false);
        }
    }

    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    if (fps_time_ != std::chrono::steady_clock::time_point())
    {
        const std::chrono::milliseconds duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - fps_time_);
        fps_ = calculateFps(fps_, duration, fps_frame_count_);
    }
    else
    {
        fps_ = 0;
    }

    fps_time_ = now;
    fps_frame_count_ = 0;

    Metrics metrics;
    metrics.packet_count = packet_count_;
    if (min_packet_ != std::numeric_limits<size_t>::max())
        metrics.min_packet = min_packet_;
    metrics.max_packet = max_packet_;
    metrics.avg_packet = avg_packet_;
    metrics.fps = fps_;
    metrics.capturer_type = capturer_type_;
    metrics.encoder_type = encoder_type_;
    metrics.hardware_decoder = hardware_decoder_;
    metrics.cursor_shape_count = cursor_shape_count_;
    metrics.cursor_pos_count = cursor_pos_count_;
    metrics.cursor_cached = cursor_cached_;
    metrics.cursor_taken_from_cache = cursor_taken_from_cache_;

    emit sig_metrics(metrics);
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::decodePacket(const proto::video::Packet& packet)
{
    proto::video::ErrorCode error_code = packet.error_code();
    if (error_code != proto::video::ERROR_CODE_OK)
    {
        LOG(ERROR) << "Video error detected:" << error_code;
        emit sig_frameError(error_code);
        return;
    }

    if (encoding_ != packet.encoding())
    {
        LOG(INFO) << "Video encoding changed from" << encoding_ << "to" << packet.encoding();
        decoder_ = VideoDecoder::create(packet.encoding(), h264_hw_enabled_);
        encoding_ = packet.encoding();
        key_frame_received_ = false;
    }

    if (!decoder_)
    {
        LOG(ERROR) << "Video decoder is not initialized";
        return;
    }

    if (packet.has_format())
    {
        const proto::video::PacketFormat& format = packet.format();

        LOG(INFO) << "Video packet has format:" << format;
        key_frame_received_ = true;

        QSize video_size = QSize(format.video_rect().width(), format.video_rect().height());
        QSize screen_size = video_size;

        static const int kMaxValue = std::numeric_limits<quint16>::max();

        if (video_size.width()  <= 0 || video_size.width()  >= kMaxValue ||
            video_size.height() <= 0 || video_size.height() >= kMaxValue)
        {
            LOG(ERROR) << "Wrong video frame size:" << video_size;
            return;
        }

        if (format.has_screen_size())
        {
            screen_size = QSize(format.screen_size().width(), format.screen_size().height());

            if (screen_size.width() <= 0 || screen_size.width() >= kMaxValue ||
                screen_size.height() <= 0 || screen_size.height() >= kMaxValue)
            {
                LOG(ERROR) << "Wrong screen size:" << screen_size;
                return;
            }
        }

        capturer_type_ = static_cast<quint32>(format.capturer_type());
        screen_size_ = screen_size;

        encoder_type_ = static_cast<quint32>(encoding_);
        hardware_decoder_ = decoder_->isHardwareAccelerated();
    }

    if (packet.flags() & proto::video::PACKET_FLAG_IS_KEY_FRAME)
        key_frame_received_ = true;

    if (!key_frame_received_)
    {
        // Until a keyframe is not received, we drop all video packets.
        LOG(INFO) << "Video packet is dropped";
        return;
    }

    const VideoDecoder::Result decode_result = decoder_->decode(packet);
    if (decode_result == VideoDecoder::Result::PERMANENT_ERROR)
    {
        if (encoding_ != proto::video::ENCODING_H264)
            return;

        if (h264_hw_enabled_)
        {
            // First permanent failure - HW decoder cannot proceed. Swap in OpenH264 SW decoder
            // and ask the host for a keyframe so the new decoder starts from a known state.
            LOG(WARNING) << "Permanent HW H264 decoder failure. Falling back to SW H264";
            h264_hw_enabled_ = false;
            decoder_ = VideoDecoder::create(encoding_, false);
            key_frame_received_ = false;

            encoder_type_ = static_cast<quint32>(encoding_);
            hardware_decoder_ = decoder_ && decoder_->isHardwareAccelerated();
            sendKeyFrameRequest();
        }
        else if (h264_sw_enabled_)
        {
            // SW decoder also failed - H264 just cannot handle this stream (resolution above the
            // level limits is the typical reason). Drop H264 from capabilities and re-negotiate;
            // host will pick VP8 and the decoder gets recreated on the encoding change.
            LOG(WARNING) << "Permanent SW H264 decoder failure. Disabling H264";
            h264_sw_enabled_ = false;
            decoder_.reset();

            emit sig_h264Disabled();
        }
        return;
    }

    if (decode_result == VideoDecoder::Result::TEMPORARY_ERROR)
    {
        LOG(ERROR) << "Unable to decode video packet";
        key_frame_received_ = false;

        sendKeyFrameRequest();
        enableForceReliable();
        return;
    }

    const int rect_count = packet.dirty_rect_size();
    if (dirty_rects_.capacity() < rect_count)
        dirty_rects_.reserve(rect_count);

    dirty_rects_.resize(rect_count);
    for (int i = 0; i < rect_count; ++i)
        dirty_rects_[i] = parse(packet.dirty_rect(i));

    const YuvConverter::Result convert_result =
        yuv_converter_.convert(decoder_->frame(), dirty_rects_);
    if (convert_result == YuvConverter::Result::FAILED)
    {
        LOG(ERROR) << "Unable to convert frame";
        return;
    }

    if (convert_result == YuvConverter::Result::NEW_FRAME)
        emit sig_frameChanged(screen_size_, yuv_converter_.frame());

    ++packet_count_;
    ++fps_frame_count_;

    const size_t packet_size = packet.ByteSizeLong();
    avg_packet_ = calculateAvgSize(avg_packet_, packet_size);
    min_packet_ = std::min(min_packet_, packet_size);
    max_packet_ = std::max(max_packet_, packet_size);

    emit sig_drawFrame(dirty_rects_);
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::sendKeyFrameRequest()
{
    // The legacy protocol has no keyframe request; the host sends keyframes on its own schedule.
    if (legacy_)
        return;

    proto::video::ClientToHost message;
    message.mutable_key_frame()->set_dummy(1);
    emit sig_sendMessage(proto::desktop::CHANNEL_ID_VIDEO, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::enableForceReliable()
{
    if (!force_reliable_active_ && reliable_disable_count_ < 2)
    {
        LOG(INFO) << "Enabling force reliable (disable count:" << reliable_disable_count_ << ")";
        force_reliable_active_ = true;
        sendForceReliable(true);
    }
    reliable_hold_seconds_ = 60;
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::sendForceReliable(bool enable)
{
    // The legacy protocol has no such feedback command.
    if (legacy_)
        return;

    proto::control::ClientToHost message;
    proto::control::Feedback* feedback = message.mutable_feedback();
    feedback->set_command_name("reliable");
    feedback->set_boolean(enable);
    emit sig_sendMessage(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::readCursorShape(const proto::cursor::Shape& shape)
{
    if (!cursor_shape_enabled_)
    {
        LOG(ERROR) << "Cursor shape received not disabled in client";
        return;
    }

    ++cursor_shape_count_;

    if (!cursor_decoder_)
    {
        LOG(INFO) << "Cursor decoder initialization";
        cursor_decoder_ = std::make_unique<CursorDecoder>();
    }

    std::shared_ptr<MouseCursor> mouse_cursor = cursor_decoder_->decode(shape);
    if (mouse_cursor)
        emit sig_mouseCursorChanged(std::move(mouse_cursor));

    cursor_cached_ = cursor_decoder_->cachedCursors();
    cursor_taken_from_cache_ = cursor_decoder_->takenCursorsFromCache();
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::readCursorPosition(const proto::cursor::Position& position)
{
    if (!cursor_position_enabled_)
        return;

    ++cursor_pos_count_;
    emit sig_cursorPositionChanged(position);
}
