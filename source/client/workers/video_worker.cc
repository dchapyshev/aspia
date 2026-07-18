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

#include <limits>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/codec/cursor_decoder.h"
#include "base/codec/video_decoder.h"
#include "base/codec/webm_file_writer.h"
#include "base/codec/webm_video_encoder.h"
#include "base/desktop/mouse_cursor.h"
#include "base/threading/worker_manager.h"
#include "client/workers/network_worker.h"
#include "proto/desktop_control.h"

namespace {

//--------------------------------------------------------------------------------------------------
int calculateFps(int last_fps, const Worker::Milliseconds& duration, qint64 count)
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
    return size_t((kAlpha * double(bytes)) + ((1.0 - kAlpha) * double(last_avg_size)));
}

} // namespace

//--------------------------------------------------------------------------------------------------
VideoWorker::VideoWorker()
    : Worker(Thread::AsioDispatcher, Seconds(1))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
VideoWorker::~VideoWorker()
{
    LOG(INFO) << "Dtor";
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
void VideoWorker::onSetRecording(bool enable, const QString& file_path, const QString& computer_name)
{
    if (enable)
    {
        LOG(INFO) << "Recording enabled:" << file_path;
        writer_ = std::make_unique<WebmFileWriter>(file_path, computer_name);
        encoder_ = std::make_unique<WebmVideoEncoder>();
        encode_timer_->start();
    }
    else
    {
        LOG(INFO) << "Recording disabled";
        encode_timer_->stop();
        encoder_.reset();
        writer_.reset();
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
        // Legacy channel (CHANNEL_ID_LEGACY == 0), where old hosts multiplex video, cursor and audio.
        connect(network_worker, &NetworkWorker::sig_channel_0, this, &VideoWorker::onLegacyMessage,
                Qt::QueuedConnection);
        // Audio channel (CHANNEL_ID_AUDIO == 7); parsed only while recording, to mux into the file.
        connect(network_worker, &NetworkWorker::sig_channel_7, this, &VideoWorker::onAudioMessage,
                Qt::QueuedConnection);
        connect(this, &VideoWorker::sig_sendMessage, network_worker, &NetworkWorker::onSendMessage,
                Qt::QueuedConnection);
    }
    else
    {
        LOG(ERROR) << "Network worker not found";
    }

    encode_timer_ = new QTimer(this);
    encode_timer_->setInterval(Milliseconds(60));
    connect(encode_timer_, &QTimer::timeout, this, &VideoWorker::onEncodeTimer);
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::onStop()
{
    LOG(INFO) << "Video worker stopped";

    encode_timer_->stop();
    encode_timer_.reset();

    encoder_.reset();
    writer_.reset();
    decoder_.reset();
    cursor_decoder_.reset();
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::onTimer()
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

    const TimePoint now = Clock::now();

    if (fps_time_ != TimePoint())
    {
        const Milliseconds duration = std::chrono::duration_cast<Milliseconds>(now - fps_time_);
        metrics_.fps = calculateFps(metrics_.fps, duration, fps_frame_count_);
    }
    else
    {
        metrics_.fps = 0;
    }

    fps_time_ = now;
    fps_frame_count_ = 0;

    emit sig_metrics(metrics_);
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
void VideoWorker::onLegacyMessage(const QByteArray& buffer)
{
    legacy_ = true;

    proto::legacy::SessionToClient* message =
        incoming_message_.parse<proto::legacy::SessionToClient>(buffer);
    if (!message)
    {
        LOG(ERROR) << "Unable to parse legacy message";
        return;
    }

    if (message->has_video_packet())
        decodePacket(message->video_packet());
    if (message->has_cursor_shape())
        readCursorShape(message->cursor_shape());
    if (message->has_cursor_position())
        readCursorPosition(message->cursor_position());
    if (writer_ && message->has_audio_packet())
        writer_->addAudioPacket(message->audio_packet());
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
void VideoWorker::onAudioMessage(const QByteArray& buffer)
{
    if (!writer_)
        return;

    proto::audio::HostToClient* message = incoming_message_.parse<proto::audio::HostToClient>(buffer);
    if (!message)
    {
        LOG(ERROR) << "Unable to parse audio message";
        return;
    }

    writer_->addAudioPacket(message->packet());
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::onEncodeTimer()
{
    // Re-encode the latest decoded frame directly from its YUV form. The decoder holds the frame
    // until the next decode(); both run on this thread, so the view is valid here without a copy.
    if (!writer_ || !encoder_ || !decoder_ || !decoder_->frame().isValid())
        return;

    proto::video::Packet packet;
    if (encoder_->encode(decoder_->frame(), &packet))
        writer_->addVideoPacket(packet);
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

        metrics_.capturer_type = static_cast<quint32>(format.capturer_type());
        screen_size_ = screen_size;

        metrics_.encoder_type = static_cast<quint32>(encoding_);
        metrics_.hardware_decoder = decoder_->isHardwareAccelerated();
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

            metrics_.encoder_type = static_cast<quint32>(encoding_);
            metrics_.hardware_decoder = decoder_ && decoder_->isHardwareAccelerated();
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

    ++fps_frame_count_;

    const size_t packet_size = packet.ByteSizeLong();

    // Before the first packet min stays 0; from the first packet on it tracks the real minimum.
    metrics_.min_packet = (metrics_.packet_count == 0) ?
        packet_size : std::min(metrics_.min_packet, packet_size);
    metrics_.max_packet = std::max(metrics_.max_packet, packet_size);
    metrics_.avg_packet = calculateAvgSize(metrics_.avg_packet, packet_size);
    ++metrics_.packet_count;

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

    ++metrics_.cursor_shape_count;

    if (!cursor_decoder_)
    {
        LOG(INFO) << "Cursor decoder initialization";
        cursor_decoder_ = std::make_unique<CursorDecoder>();
    }

    std::shared_ptr<MouseCursor> mouse_cursor = cursor_decoder_->decode(shape);
    if (mouse_cursor)
        emit sig_mouseCursorChanged(std::move(mouse_cursor));

    metrics_.cursor_cached = cursor_decoder_->cachedCursors();
    metrics_.cursor_taken_from_cache = cursor_decoder_->takenCursorsFromCache();
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::readCursorPosition(const proto::cursor::Position& position)
{
    if (!cursor_position_enabled_)
        return;

    ++metrics_.cursor_pos_count;
    emit sig_cursorPositionChanged(position);
}
