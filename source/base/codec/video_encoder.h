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

#ifndef BASE_CODEC_VIDEO_ENCODER_H
#define BASE_CODEC_VIDEO_ENCODER_H

#include <QtTypes>

#include <memory>
#include <string>

namespace proto::video {
enum Encoding : int;
class Packet;
} // namespace proto::video

class Frame;

class VideoEncoder
{
public:
    static std::unique_ptr<VideoEncoder> create(proto::video::Encoding encoding);

    virtual ~VideoEncoder() = default;

    static const size_t kInitialEncodeBufferSize;

    virtual bool encode(const Frame* frame, proto::video::Packet* packet) = 0;

    proto::video::Encoding encoding() const { return encoding_; }

    void setKeyFrameRequired(bool enable) { key_frame_required_ = enable; }
    bool isKeyFrameRequired() const { return key_frame_required_; }
    void setEncodeBuffer(std::string&& buffer) { encode_buffer_ = std::move(buffer); }

    bool setMinQuantizer(quint32 min_quantizer);
    quint32 minQuantizer() const { return min_quantizer_; }
    bool setMaxQuantizer(quint32 max_quantizer);
    quint32 maxQuantizer() const { return max_quantizer_; }

protected:
    explicit VideoEncoder(proto::video::Encoding encoding);

    // Applies the current min/max quantizer to a live codec instance. Returns true also when
    // there is no active codec yet - the new value is read on next codec (re)creation.
    virtual bool applyMinQuantizer() = 0;
    virtual bool applyMaxQuantizer() = 0;

    const proto::video::Encoding encoding_;
    bool key_frame_required_ = false;
    std::string encode_buffer_;
    quint32 min_quantizer_ = 10;
    quint32 max_quantizer_ = 30;

private:
    Q_DISABLE_COPY_MOVE(VideoEncoder)
};

#endif // BASE_CODEC_VIDEO_ENCODER_H
