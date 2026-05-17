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
    Q_GADGET

public:
    enum class Result
    {
        SUCCESS,         // Packet was produced and is ready to send.
        TEMPORARY_ERROR, // Recoverable failure (transient driver/codec hiccup); retry next frame.
        PERMANENT_ERROR, // Encoder cannot proceed (e.g. HW does not support this frame size);
                         // the caller should fall back to another encoding.
    };
    Q_ENUM(Result)

    static std::unique_ptr<VideoEncoder> create(proto::video::Encoding encoding);

    // Returns true when the encoding can be instantiated on this system. Used by callers to pick
    // a supported encoding upfront without paying the cost of constructing and tearing down an
    // encoder just to probe availability.
    static bool isSupported(proto::video::Encoding encoding);

    virtual ~VideoEncoder() = default;

    static const size_t kInitialEncodeBufferSize;

    virtual Result encode(const Frame* frame, proto::video::Packet* packet) = 0;

    // Adjusts internal codec parameters (target bitrate, quantizer bounds, etc.) so the encoder
    // produces output that fits the available network bandwidth. Bandwidth is measured in bytes
    // per second; pass 0 when the bandwidth has not been measured yet to fall back to defaults.
    // Each derived class translates the bandwidth into the right knobs for its codec.
    virtual void setBandwidth(qint64 bandwidth) = 0;

    proto::video::Encoding encoding() const { return encoding_; }

    void setKeyFrameRequired(bool enable) { key_frame_required_ = enable; }
    bool isKeyFrameRequired() const { return key_frame_required_; }
    void setEncodeBuffer(std::string&& buffer) { encode_buffer_ = std::move(buffer); }

protected:
    explicit VideoEncoder(proto::video::Encoding encoding);

    const proto::video::Encoding encoding_;
    bool key_frame_required_ = false;
    std::string encode_buffer_;

private:
    Q_DISABLE_COPY_MOVE(VideoEncoder)
};

#endif // BASE_CODEC_VIDEO_ENCODER_H
