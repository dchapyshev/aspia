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

#include <mkvmuxer/mkvmuxer.h>
#include <mkvmuxer/mkvwriter.h>

#include "base/logging.h"
#include "proto/desktop_audio.h"
#include "proto/desktop_video.h"

//--------------------------------------------------------------------------------------------------
WebmFileWriter::WebmFileWriter(const QString& path, const QString& name)
    : path_(path),
      name_(name),
      last_video_encoding_(proto::video::ENCODING_UNKNOWN)
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
void WebmFileWriter::addVideoPacket(const proto::video::Packet& packet)
{
    if (packet.encoding() != last_video_encoding_ || packet.has_format())
    {
        close();

        switch (packet.encoding())
        {
            case proto::video::ENCODING_VP8:
            case proto::video::ENCODING_VP9:
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
        if (packet.encoding() == proto::video::ENCODING_VP9)
            video_codec_id = mkvmuxer::Tracks::kVp9CodecId;

        if (!addVideoTrack(packet.format().video_rect().width(),
                           packet.format().video_rect().height(),
                           video_codec_id))
        {
            return;
        }

        if (!addAudioTrack(proto::audio::Packet::SAMPLING_RATE_48000,
                           proto::audio::Packet::CHANNELS_STEREO,
                           mkvmuxer::Tracks::kOpusCodecId))
        {
            return;
        }

        is_key_frame = true;
    }

    if (!segment_)
        return;

    DCHECK(video_track_num_);
    DCHECK(audio_track_num_);

    TimePoint current = Clock::now();
    NanoSeconds timestamp;

    if (video_start_time_.has_value())
    {
        timestamp = DurationCast<NanoSeconds>(current - *video_start_time_);
    }
    else
    {
        video_start_time_.emplace(current);
        timestamp = NanoSeconds(0);
    }

    writeFrame(packet.data(), timestamp, video_track_num_, is_key_frame);
}

//--------------------------------------------------------------------------------------------------
void WebmFileWriter::addAudioPacket(const proto::audio::Packet& packet)
{
    if (packet.encoding() != proto::audio::ENCODING_OPUS ||
        packet.channels() != proto::audio::Packet::CHANNELS_STEREO ||
        packet.sampling_rate() != proto::audio::Packet::SAMPLING_RATE_48000)
    {
        // Unsupported audio packet.
        return;
    }

    if (!segment_ || !audio_track_num_)
        return;

    for (int i = 0; i < packet.data_size(); ++i)
    {
        TimePoint current = Clock::now();
        NanoSeconds timestamp;

        if (video_start_time_.has_value())
        {
            timestamp = DurationCast<NanoSeconds>(current - *video_start_time_);
        }
        else
        {
            video_start_time_.emplace(current);
            timestamp = NanoSeconds(0);
        }

        writeFrame(packet.data(i), timestamp, audio_track_num_, false);
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

    QString time = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss.zzz");
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

    mkv_writer_ = std::make_unique<mkvmuxer::MkvWriter>(file_);
    segment_ = std::make_unique<mkvmuxer::Segment>();

    if (!segment_->Init(mkv_writer_.get()))
    {
        LOG(ERROR) << "Cannot init segment";
        close();
        return false;
    }

    segment_->set_mode(mkvmuxer::Segment::kFile);

    mkvmuxer::SegmentInfo* const segment_info = segment_->GetSegmentInfo();
    if (!segment_info)
    {
        LOG(ERROR) << "Segment has no SegmentInfo";
        close();
        return false;
    }

    segment_info->set_writing_app("Aspia");

    ++file_counter_;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool WebmFileWriter::addAudioTrack(int sample_rate, int channels, std::string_view codec_id)
{
    if (audio_track_num_ != 0)
    {
        LOG(ERROR) << "Cannot add audio track: it already exists";
        return false;
    }

    if (codec_id.empty())
    {
        LOG(ERROR) << "Cannot add audio track with empty codec id";
        return false;
    }

    audio_track_num_ = segment_->AddAudioTrack(sample_rate, channels, 0);
    if (!audio_track_num_)
    {
        LOG(ERROR) << "Cannot add audio track on segment";
        return false;
    }

    mkvmuxer::AudioTrack* const audio_track = static_cast<mkvmuxer::AudioTrack*>(
        segment_->GetTrackByNumber(audio_track_num_));
    if (!audio_track)
    {
        LOG(ERROR) << "Unable to set audio codec id: track look up failed";
        return false;
    }

    audio_track->set_codec_id(codec_id.data());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool WebmFileWriter::addVideoTrack(int width, int height, std::string_view codec_id)
{
    if (video_track_num_ != 0)
    {
        LOG(ERROR) << "Cannot add video track: it already exists";
        return false;
    }

    if (codec_id.empty())
    {
        LOG(ERROR) << "Cannot add video track with empty codec id";
        return false;
    }

    video_track_num_ = segment_->AddVideoTrack(width, height, 0);
    if (!video_track_num_)
    {
        LOG(ERROR) << "Cannot add video track on segment";
        return false;
    }

    mkvmuxer::VideoTrack* const video_track = static_cast<mkvmuxer::VideoTrack*>(
        segment_->GetTrackByNumber(video_track_num_));
    if (!video_track)
    {
        LOG(ERROR) << "Unable to set video codec id: track look up failed";
        return false;
    }

    video_track->set_codec_id(codec_id.data());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool WebmFileWriter::writeFrame(
    std::string_view frame, NanoSeconds timestamp, quint64 track_num, bool is_key)
{
    if (!segment_->AddFrame(reinterpret_cast<const quint8*>(frame.data()), frame.size(),
                            track_num, static_cast<quint64>(timestamp.count()), is_key))
    {
        LOG(ERROR) << "AddFrame failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void WebmFileWriter::close()
{
    last_video_encoding_ = proto::video::ENCODING_UNKNOWN;
    video_start_time_.reset();
    audio_start_time_.reset();

    if (segment_)
    {
        // Everything libwebm has buffered goes out here, so this must happen before the file is
        // closed under it.
        if (!segment_->Finalize())
            LOG(ERROR) << "Segment finalize failed";

        segment_.reset();
    }

    mkv_writer_.reset();

    audio_track_num_ = 0;
    video_track_num_ = 0;

    if (file_)
    {
        LOG(INFO) << "File closed";
        fclose(file_);
        file_ = nullptr;
    }
}
