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

#include "base/codec/webm_file_writer.h"

#include <QDateTime>
#include <QDir>

#include "base/logging.h"
#include "base/codec/webm_file_muxer.h"

namespace base {

//--------------------------------------------------------------------------------------------------
WebmFileWriter::WebmFileWriter(const QString& path, const QString& name)
    : path_(path),
      name_(name)
{
    LOG(INFO) << "Ctor (path=" << path << "name=" << name.data() << ")";
}

//--------------------------------------------------------------------------------------------------
WebmFileWriter::~WebmFileWriter()
{
    LOG(INFO) << "Dtor";
    close();
}

//--------------------------------------------------------------------------------------------------
void WebmFileWriter::addVideoPacket(const proto::desktop::VideoPacket& packet)
{
    if (packet.encoding() != last_video_encoding_ || packet.has_format())
    {
        close();

        switch (packet.encoding())
        {
            case proto::desktop::VIDEO_ENCODING_VP8:
            case proto::desktop::VIDEO_ENCODING_VP9:
                break;

            default:
                LOG(ERROR) << "Not supported video encoding";
                return;
        }

        last_video_encoding_ = packet.encoding();

        if (!packet.has_format())
            return;
    }

    bool is_key_frame = false;

    if (packet.has_format())
    {
        if (!init())
        {
            LOG(ERROR) << "init failed";
            return;
        }

        const char* video_codec_id = mkvmuxer::Tracks::kVp8CodecId;
        if (packet.encoding() == proto::desktop::VIDEO_ENCODING_VP9)
            video_codec_id = mkvmuxer::Tracks::kVp9CodecId;

        if (!muxer_->addVideoTrack(packet.format().video_rect().width(),
                                   packet.format().video_rect().height(),
                                   video_codec_id))
        {
            LOG(ERROR) << "WebmFileMuxer::addVideoTrack failed";
            return;
        }

        if (!muxer_->addAudioTrack(proto::desktop::AudioPacket::SAMPLING_RATE_48000,
                                   proto::desktop::AudioPacket::CHANNELS_STEREO,
                                   mkvmuxer::Tracks::kOpusCodecId))
        {
            LOG(ERROR) << "WebmFileMuxer::addAudioTrack failed";
            return;
        }

        is_key_frame = true;
    }

    if (!muxer_)
        return;

    DCHECK(muxer_->hasVideoTrack());
    DCHECK(muxer_->hasAudioTrack());

    TimePoint current = Clock::now();
    NanoSeconds timestamp;

    if (video_start_time_.has_value())
    {
        timestamp = std::chrono::duration_cast<NanoSeconds>(current - *video_start_time_);
    }
    else
    {
        video_start_time_.emplace(current);
        timestamp = NanoSeconds(0);
    }

    muxer_->writeVideoFrame(packet.data(), timestamp, is_key_frame);
}

//--------------------------------------------------------------------------------------------------
void WebmFileWriter::addAudioPacket(const proto::desktop::AudioPacket& packet)
{
    if (packet.encoding() != proto::desktop::AUDIO_ENCODING_OPUS ||
        packet.channels() != proto::desktop::AudioPacket::CHANNELS_STEREO ||
        packet.sampling_rate() != proto::desktop::AudioPacket::SAMPLING_RATE_48000)
    {
        // Unsupported audio packet.
        return;
    }

    if (!muxer_ || !muxer_->hasAudioTrack())
        return;

    for (int i = 0; i < packet.data_size(); ++i)
    {
        TimePoint current = Clock::now();
        NanoSeconds timestamp;

        if (video_start_time_.has_value())
        {
            timestamp = std::chrono::duration_cast<NanoSeconds>(
                current - *video_start_time_);
        }
        else
        {
            video_start_time_.emplace(current);
            timestamp = NanoSeconds(0);
        }

        muxer_->writeAudioFrame(packet.data(i), timestamp);
    }
}

//--------------------------------------------------------------------------------------------------
bool WebmFileWriter::init()
{
    QDir directory(path_);

    if (!directory.exists())
    {
        LOG(INFO) << "Path" << path_ << "not exists yet";

        if (directory.mkpath(path_))
        {
            LOG(INFO) << "Path created successfully";
        }
        else
        {
            LOG(ERROR) << "Unable to create path";
            return false;
        }
    }
    else
    {
        LOG(INFO) << "Path" << path_ << "already exists";
    }

    QString time = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss.zzz"));
    QString file_name = QString("/%1-%2.%3.webm").arg(name_, time).arg(file_counter_);
    QString file_path = path_ + file_name;

    LOG(INFO) << "New video file:" << file_path;

#if defined(Q_OS_WINDOWS)
    if (fopen_s(&file_, file_path.toLocal8Bit().data(), "wb") != 0)
#else
    file_ = fopen(file_path.toLocal8Bit().data(), "wb");
    if (!file_)
#endif
    {
        LOG(ERROR) << "Could not open file for writing";
        return false;
    }

    muxer_ = std::make_unique<WebmFileMuxer>();
    if (!muxer_->init(file_))
    {
        LOG(ERROR) << "WebmFileMuxer::init failed";
        close();
        return false;
    }

    ++file_counter_;
    return true;
}

//--------------------------------------------------------------------------------------------------
void WebmFileWriter::close()
{
    last_video_encoding_ = proto::desktop::VIDEO_ENCODING_UNKNOWN;
    video_start_time_.reset();
    audio_start_time_.reset();

    if (muxer_)
    {
        if (muxer_->initialized() && !muxer_->finalize())
        {
            LOG(ERROR) << "WebmFileMuxer::finalize failed";
        }

        LOG(INFO) << "Muxer destroyed";
        muxer_.reset();
    }

    if (file_)
    {
        LOG(INFO) << "File closed";
        fclose(file_);
        file_ = nullptr;
    }
}

} // namespace base
