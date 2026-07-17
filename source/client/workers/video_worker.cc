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
#include "base/codec/video_decoder.h"
#include "base/codec/webm_video_encoder.h"

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
void VideoWorker::onVideoPacket(std::shared_ptr<proto::video::Packet> packet)
{
    if (!packet)
        return;

    proto::video::ErrorCode error_code = packet->error_code();
    if (error_code != proto::video::ERROR_CODE_OK)
    {
        LOG(ERROR) << "Video error detected:" << error_code;
        emit sig_frameError(error_code);
        return;
    }

    if (encoding_ != packet->encoding())
    {
        LOG(INFO) << "Video encoding changed from" << encoding_ << "to" << packet->encoding();
        decoder_ = VideoDecoder::create(packet->encoding(), h264_hw_enabled_);
        encoding_ = packet->encoding();
        key_frame_received_ = false;
    }

    if (!decoder_)
    {
        LOG(ERROR) << "Video decoder is not initialized";
        return;
    }

    if (packet->has_format())
    {
        const proto::video::PacketFormat& format = packet->format();

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

        emit sig_videoInfoChanged(capturer_type_, static_cast<quint32>(encoding_),
                                  decoder_->isHardwareAccelerated());
    }

    if (packet->flags() & proto::video::PACKET_FLAG_IS_KEY_FRAME)
        key_frame_received_ = true;

    if (!key_frame_received_)
    {
        // Until a keyframe is not received, we drop all video packets.
        LOG(INFO) << "Video packet is dropped";
        return;
    }

    const VideoDecoder::Result decode_result = decoder_->decode(*packet);
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

            emit sig_videoInfoChanged(capturer_type_, static_cast<quint32>(encoding_),
                                      decoder_ && decoder_->isHardwareAccelerated());
            emit sig_keyFrameRequired();
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

        emit sig_temporaryError();
        return;
    }

    const int rect_count = packet->dirty_rect_size();
    if (dirty_rects_.capacity() < rect_count)
        dirty_rects_.reserve(rect_count);

    dirty_rects_.resize(rect_count);
    for (int i = 0; i < rect_count; ++i)
        dirty_rects_[i] = parse(packet->dirty_rect(i));

    const YuvConverter::Result convert_result =
        yuv_converter_.convert(decoder_->frame(), dirty_rects_);
    if (convert_result == YuvConverter::Result::FAILED)
    {
        LOG(ERROR) << "Unable to convert frame";
        return;
    }

    if (convert_result == YuvConverter::Result::NEW_FRAME)
        emit sig_frameChanged(screen_size_, yuv_converter_.frame());

    emit sig_drawFrame(dirty_rects_, packet->ByteSizeLong());
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::onSetRecording(bool enable)
{
    if (enable)
    {
        LOG(INFO) << "Video recording is enabled";
        recording_encoder_ = std::make_unique<WebmVideoEncoder>();
        recording_timer_->start();
    }
    else
    {
        LOG(INFO) << "Video recording is disabled";
        recording_timer_->stop();
        recording_encoder_.reset();
    }
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::onStart()
{
    LOG(INFO) << "Video worker started";

    recording_timer_ = new QTimer(this);
    recording_timer_->setInterval(std::chrono::milliseconds(60));

    connect(recording_timer_, &QTimer::timeout, this, [this]()
    {
        if (!recording_encoder_ || !decoder_ || !decoder_->frame().isValid())
            return;

        std::shared_ptr<proto::video::Packet> packet = std::make_shared<proto::video::Packet>();

        if (recording_encoder_->encode(decoder_->frame(), packet.get()))
            emit sig_recordingVideoPacket(std::move(packet));
    });
}

//--------------------------------------------------------------------------------------------------
void VideoWorker::onStop()
{
    LOG(INFO) << "Video worker stopped";

    delete recording_timer_;
    recording_timer_ = nullptr;

    recording_encoder_.reset();
    decoder_.reset();
}
