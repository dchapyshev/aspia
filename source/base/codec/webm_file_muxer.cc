//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/codec/webm_file_muxer.h"

#include "base/logging.h"

#include <cstdio>
#include <cstring>

#include "third_party/libwebm/webmids.hpp"

namespace base {

//--------------------------------------------------------------------------------------------------
WebmFileMuxer::WebmFileMuxer() = default;

//--------------------------------------------------------------------------------------------------
WebmFileMuxer::~WebmFileMuxer() = default;

//--------------------------------------------------------------------------------------------------
bool WebmFileMuxer::init(FILE* file)
{
    DCHECK(file);

    // Construct and Init |WebMChunkWriter|. It handles writes coming from libwebm.
    writer_ = std::make_unique<mkvmuxer::MkvWriter>(file);

    // Construct and init |ptr_segment_|, then enable live mode.
    segment_ = std::make_unique<mkvmuxer::Segment>();

    if (!segment_->Init(writer_.get()))
    {
        LOG(LS_ERROR) << "Cannot Init Segment";
        return false;
    }

    segment_->set_mode(mkvmuxer::Segment::kFile);

    // Set segment info fields.
    mkvmuxer::SegmentInfo* const segment_info = segment_->GetSegmentInfo();
    if (!segment_info)
    {
        LOG(LS_ERROR) << "Segment has no SegmentInfo";
        return false;
    }

    // Set writing application name.
    segment_info->set_writing_app("WebmFileMuxer");

    initialized_ = true;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool WebmFileMuxer::addAudioTrack(int sample_rate, int channels, std::string_view codec_id)
{
    if (audio_track_num_ != 0)
    {
        LOG(LS_ERROR) << "Cannot add audio track: it already exists";
        return false;
    }

    if (codec_id.empty())
    {
        LOG(LS_ERROR) << "Cannot AddAudioTrack with empty codec_id";
        return false;
    }

    audio_track_num_ = segment_->AddAudioTrack(sample_rate, channels, 0);
    if (!audio_track_num_)
    {
        LOG(LS_ERROR) << "Cannot AddAudioTrack on segment";
        return false;
    }

    mkvmuxer::AudioTrack* const audio_track = static_cast<mkvmuxer::AudioTrack*>(
        segment_->GetTrackByNumber(audio_track_num_));
    if (!audio_track)
    {
        LOG(LS_ERROR) << "Unable to set audio codec_id: Track look up failed";
        return false;
    }

    audio_track->set_codec_id(codec_id.data());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool WebmFileMuxer::addVideoTrack(int width, int height, std::string_view codec_id)
{
    if (video_track_num_ != 0)
    {
        LOG(LS_ERROR) << "Cannot add video track: it already exists";
        return false;
    }

    if (codec_id.empty())
    {
        LOG(LS_ERROR) << "Cannot AddVideoTrack with empty codec_id";
        return false;
    }

    video_track_num_ = segment_->AddVideoTrack(width, height, 0);
    if (!video_track_num_)
    {
        LOG(LS_ERROR) << "Cannot AddVideoTrack on segment";
        return false;
    }

    mkvmuxer::VideoTrack* const video_track = static_cast<mkvmuxer::VideoTrack*>(
       segment_->GetTrackByNumber(video_track_num_));
    if (!video_track)
    {
        LOG(LS_ERROR) << "Unable to set video codec_id: Track look up failed";
        return false;
    }

    video_track->set_codec_id(codec_id.data());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool WebmFileMuxer::finalize()
{
    if (!segment_->Finalize())
    {
        LOG(LS_ERROR) << "libwebm mkvmuxer Finalize failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool WebmFileMuxer::writeAudioFrame(std::string_view frame,
                                    const std::chrono::nanoseconds& timestamp)
{
    if (audio_track_num_ == 0)
    {
        LOG(LS_ERROR) << "Cannot WriteAudioFrame without an audio track";
        return false;
    }

    return writeFrame(frame, timestamp, audio_track_num_, false);
}

//--------------------------------------------------------------------------------------------------
bool WebmFileMuxer::writeVideoFrame(std::string_view frame,
                                    const std::chrono::nanoseconds& timestamp,
                                    bool is_key)
{
    if (video_track_num_ == 0)
    {
        LOG(LS_ERROR) << "Cannot WriteVideoFrame without a video track";
        return false;
    }

    return writeFrame(frame, timestamp, video_track_num_, is_key);
}

//--------------------------------------------------------------------------------------------------
bool WebmFileMuxer::writeFrame(std::string_view frame,
                               const std::chrono::nanoseconds& timestamp,
                               quint64 track_num, bool is_key)
{
    if (!segment_->AddFrame(reinterpret_cast<const quint8*>(frame.data()), frame.size(),
                            track_num, static_cast<quint64>(timestamp.count()), is_key))
    {
        LOG(LS_ERROR) << "AddFrame failed";
        return false;
    }

    return true;
}

} // namespace base
