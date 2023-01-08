//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/macros_magic.h"
#include "base/desktop/geometry.h"
#include "base/memory/byte_array.h"
#include "proto/desktop.pb.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>

namespace base {

class WebmFileMuxer;

class WebmFileWriter
{
public:
    WebmFileWriter(const std::filesystem::path& path, std::u16string_view name);
    ~WebmFileWriter();

    void addVideoPacket(const proto::VideoPacket& packet);
    void addAudioPacket(const proto::AudioPacket& packet);

private:
    bool init();
    void close();

    std::filesystem::path path_;
    std::u16string name_;
    int file_counter_ = 0;
    FILE* file_ = nullptr;

    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using NanoSeconds = std::chrono::nanoseconds;

    std::unique_ptr<WebmFileMuxer> muxer_;
    std::optional<TimePoint> video_start_time_;
    std::optional<TimePoint> audio_start_time_;

    proto::VideoEncoding last_video_encoding_ = proto::VIDEO_ENCODING_UNKNOWN;

    DISALLOW_COPY_AND_ASSIGN(WebmFileWriter);
};

} // namespace base

#endif // BASE_CODEC_WEBM_FILE_WRITER_H
