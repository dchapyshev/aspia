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

#ifndef BASE_CODEC_WEBM_FILE_WRITER_H
#define BASE_CODEC_WEBM_FILE_WRITER_H

#include <QString>

#include <memory>
#include <optional>
#include <string_view>

#include "base/time_types.h"

namespace mkvmuxer {
class MkvWriter;
class Segment;
} // namespace mkvmuxer

namespace proto::audio {
class Packet;
} // namespace proto::audio

namespace proto::video {
enum Encoding : int;
class Packet;
} // namespace proto::video

class WebmFileWriter
{
public:
    WebmFileWriter(const QString& path, const QString& name);
    ~WebmFileWriter();

    void addVideoPacket(const proto::video::Packet& packet);
    void addAudioPacket(const proto::audio::Packet& packet);

private:
    // Opens the next file and starts a segment on it, in live mode.
    bool init();

    // Add a track to the open segment. Both refuse a second track of their kind.
    bool addAudioTrack(int sample_rate, int channels, std::string_view codec_id);
    bool addVideoTrack(int width, int height, std::string_view codec_id);

    bool writeFrame(std::string_view frame, NanoSeconds timestamp, quint64 track_num, bool is_key);

    // Flushes what libwebm has buffered and closes the file.
    void close();

    QString path_;
    QString name_;
    int file_counter_ = 0;
    FILE* file_ = nullptr;

    std::unique_ptr<mkvmuxer::MkvWriter> mkv_writer_;
    std::unique_ptr<mkvmuxer::Segment> segment_;
    quint64 audio_track_num_ = 0;
    quint64 video_track_num_ = 0;

    std::optional<TimePoint> video_start_time_;
    std::optional<TimePoint> audio_start_time_;

    proto::video::Encoding last_video_encoding_;

    Q_DISABLE_COPY_MOVE(WebmFileWriter)
};

#endif // BASE_CODEC_WEBM_FILE_WRITER_H
