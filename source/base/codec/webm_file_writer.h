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

#include "base/time_types.h"

class WebmFileMuxer;

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
    bool init();
    void close();

    QString path_;
    QString name_;
    int file_counter_ = 0;
    FILE* file_ = nullptr;

    std::unique_ptr<WebmFileMuxer> muxer_;
    std::optional<TimePoint> video_start_time_;
    std::optional<TimePoint> audio_start_time_;

    proto::video::Encoding last_video_encoding_;

    Q_DISABLE_COPY_MOVE(WebmFileWriter)
};

#endif // BASE_CODEC_WEBM_FILE_WRITER_H
