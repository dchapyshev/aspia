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

#ifndef BASE_CODEC_WEBM_FILE_MUXER_H
#define BASE_CODEC_WEBM_FILE_MUXER_H

#include <QtGlobal>

#include "base/macros_magic.h"

#include <chrono>
#include <memory>

#include "third_party/libwebm/mkvmuxer.hpp"
#include "third_party/libwebm/mkvwriter.hpp"

namespace base {

class WebmFileMuxer
{
public:
    WebmFileMuxer();
    ~WebmFileMuxer();

    // Initializes libwebm for muxing in live mode. Returns |true| when successful.
    bool init(FILE* file);

    bool hasAudioTrack() const { return audio_track_num_ != 0; }
    bool hasVideoTrack() const { return video_track_num_ != 0; }

    // Adds an audio track with the specified |codec_id| to |ptr_segment_|. Returns |false| when
    // adding the track to the segment fails.
    bool addAudioTrack(int sample_rate, int channels, std::string_view codec_id);

    // Adds a video track with the specified |codec_id| to |segment_|. Returns |false| when adding
    // the track to the segment fails.
    bool addVideoTrack(int width, int height, std::string_view codec_id);

    // Flushes any queued frames. Users MUST call this method to ensure that all buffered frames
    // are flushed out of libwebm.
    bool finalize();

    // Writes |data| to the audio Track.
    bool writeAudioFrame(std::string_view frame, const std::chrono::nanoseconds& timestamp);

    // Writes |data| to the video Track.
    bool writeVideoFrame(std::string_view frame,
                         const std::chrono::nanoseconds& timestamp,
                         bool is_key);

    // Accessors.
    bool initialized() const { return initialized_; }

private:
    // Writes |data| to the muxer. |size| is the size in bytes of |data|. |timestamp| is the
    // timestamp of the frame in nanoseconds. |track_num| is the Track number to add the frame.
    // |is_key| flag telling if the frame is a key frame.
    bool writeFrame(std::string_view frame,
                    const std::chrono::nanoseconds& timestamp,
                    quint64 track_num, bool is_key);

    std::unique_ptr<mkvmuxer::MkvWriter> writer_;
    std::unique_ptr<mkvmuxer::Segment> segment_;
    quint64 audio_track_num_ = 0;
    quint64 video_track_num_ = 0;
    bool initialized_ = false;

    DISALLOW_COPY_AND_ASSIGN(WebmFileMuxer);
};

} // namespace base

#endif // BASE_CODEC_WEBM_FILE_MUXER_H
