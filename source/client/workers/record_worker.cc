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

#include "client/workers/record_worker.h"

#include <QRect>
#include <QTimer>

#include "base/logging.h"
#include "base/codec/webm_file_writer.h"
#include "base/codec/webm_video_encoder.h"
#include "base/desktop/frame_aligned.h"
#include "base/threading/worker_manager.h"
#include "client/workers/network_worker.h"
#include "client/workers/video_worker.h"
#include "proto/desktop_audio.h"
#include "proto/desktop_video.h"

namespace {

// ARGB frame buffer alignment, matching the value the YUV converter allocates its frame with.
const size_t kArgbAlignment = 32;

} // namespace

//--------------------------------------------------------------------------------------------------
RecordWorker::RecordWorker()
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
RecordWorker::~RecordWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RecordWorker::onSetRecording(bool enable, const QString& file_path, const QString& computer_name)
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
        record_frame_.reset();
        // last_frame_ is kept so a recording started again immediately has a frame to encode.
    }
}

//--------------------------------------------------------------------------------------------------
void RecordWorker::onAudioMessage(const QByteArray& buffer)
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
void RecordWorker::onAudioPacket(std::shared_ptr<proto::audio::Packet> packet)
{
    if (writer_ && packet)
        writer_->addAudioPacket(*packet);
}

//--------------------------------------------------------------------------------------------------
void RecordWorker::onFrameChanged(const QSize& screen_size, SharedFrame frame)
{
    // Kept regardless of the recording state so a recording started later already has a frame; the
    // handle is a cheap reference to the buffer the GUI is already holding alive.
    last_frame_ = frame;
}

//--------------------------------------------------------------------------------------------------
void RecordWorker::onStart()
{
    LOG(INFO) << "Record worker started";

    NetworkWorker* network_worker = findWorker<NetworkWorker>();
    if (network_worker)
    {
        // Audio channel (CHANNEL_ID_AUDIO == 7); parsed only while recording.
        connect(network_worker, &NetworkWorker::sig_channel_7, this, &RecordWorker::onAudioMessage,
                Qt::QueuedConnection);
    }
    else
    {
        LOG(ERROR) << "Network worker not found";
    }

    VideoWorker* video_worker = findWorker<VideoWorker>();
    if (video_worker)
    {
        // Re-encode the same rendered ARGB frame the GUI displays.
        connect(video_worker, &VideoWorker::sig_frameChanged, this, &RecordWorker::onFrameChanged,
                Qt::QueuedConnection);
    }
    else
    {
        LOG(ERROR) << "Video worker not found";
    }

    encode_timer_ = new QTimer(this);
    encode_timer_->setInterval(std::chrono::milliseconds(60));
    connect(encode_timer_, &QTimer::timeout, this, &RecordWorker::onEncodeTimer);
}

//--------------------------------------------------------------------------------------------------
void RecordWorker::onStop()
{
    LOG(INFO) << "Record worker stopped";

    delete encode_timer_;
    encode_timer_ = nullptr;

    encoder_.reset();
    writer_.reset();
    record_frame_.reset();
    last_frame_ = SharedFrame();
}

//--------------------------------------------------------------------------------------------------
void RecordWorker::onEncodeTimer()
{
    if (!writer_ || !encoder_ || !last_frame_.isValid())
        return;

    const QSize size = last_frame_.size();
    if (!record_frame_ || record_frame_->size() != size)
    {
        record_frame_ = FrameAligned::create(size, kArgbAlignment);
        if (!record_frame_)
        {
            LOG(ERROR) << "Unable to create recording frame";
            return;
        }
    }

    // Copy the pixels out under the frame lock and encode from the private copy, so the encode does
    // not hold the lock while the video worker needs it to write the next frame.
    {
        SharedFrame::ReadAccess access = last_frame_.read();
        record_frame_->copyPixelsFrom(access.get(), QPoint(0, 0), QRect(QPoint(0, 0), size));
    }

    proto::video::Packet packet;
    if (encoder_->encode(*record_frame_, &packet))
        writer_->addVideoPacket(packet);
}
