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

#ifndef BASE_CODEC_VIDEO_ENCODER_H
#define BASE_CODEC_VIDEO_ENCODER_H

#include "base/desktop/geometry.h"
#include "proto/desktop.h"

namespace base {

class Frame;

class VideoEncoder
{
public:
    static const size_t kInitialEncodeBufferSize;

    explicit VideoEncoder(proto::desktop::VideoEncoding encoding);
    virtual ~VideoEncoder() = default;

    virtual bool encode(const Frame* frame, proto::desktop::VideoPacket* packet) = 0;

    void setKeyFrameRequired(bool enable) { key_frame_required_ = enable; }
    bool isKeyFrameRequired() const { return key_frame_required_; }
    void setEncodeBuffer(std::string&& buffer) { encode_buffer_ = std::move(buffer); }

    proto::desktop::VideoEncoding encoding() const { return encoding_; }

protected:
    void fillPacketInfo(const Frame* frame, proto::desktop::VideoPacket* packet);
    std::string* encodeBuffer() { return &encode_buffer_; }

private:
    const proto::desktop::VideoEncoding encoding_;
    Size last_size_;
    bool key_frame_required_ = false;
    std::string encode_buffer_;
};

} // namespace base

#endif // BASE_CODEC_VIDEO_ENCODER_H
