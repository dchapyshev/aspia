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

#ifndef BASE_CODEC_VIDEO_DECODER_H
#define BASE_CODEC_VIDEO_DECODER_H

#include <QSize>
#include <QtTypes>

#include <memory>

namespace proto::video {
enum Encoding : int;
class Packet;
} // namespace proto::video

class VideoDecoder
{
    Q_GADGET

public:
    enum class Result
    {
        SUCCESS,         // Frame was decoded successfully.
        TEMPORARY_ERROR, // Recoverable failure (corrupt bitstream, warm-up, etc.); retry next packet.
        PERMANENT_ERROR, // Decoder cannot proceed; the caller should fall back to another decoder.
    };
    Q_ENUM(Result)

    // Pixel format of a decoded planar frame.
    enum class YuvFormat
    {
        I420, // Three planes: Y at full resolution, U and V at half resolution.
        NV12, // Two planes: Y at full resolution, interleaved UV at half resolution.
    };
    Q_ENUM(YuvFormat)

    // Non-owning view of a decoded planar frame in I420 (Y, U, V) or NV12 (Y, UV) layout. The plane
    // pointers reference memory owned by the decoder and stay valid only until the next decode().
    class YuvView
    {
    public:
        YuvView() = default;

        // Re-points the view at a new frame of |size| in |format| and clears the plane pointers.
        // The decoder fills the view through its (non-const) frame_; outside VideoDecoder only the
        // const getters below are reachable (frame() hands out a const reference).
        void reset(YuvFormat format, const QSize& size)
        {
            format_ = format;
            size_ = size;
            plane_[0] = plane_[1] = plane_[2] = nullptr;
            stride_[0] = stride_[1] = stride_[2] = 0;
        }

        void setPlane(int index, const quint8* data, int stride)
        {
            plane_[index] = data;
            stride_[index] = stride;
        }

        bool isValid() const { return plane_[0] != nullptr; }

        YuvFormat format() const { return format_; }
        const QSize& size() const { return size_; }

        // Plane layout - I420: 0=Y, 1=U, 2=V; NV12: 0=Y, 1=UV.
        const quint8* planeData(int index) const { return plane_[index]; }
        int planeStride(int index) const { return stride_[index]; }

    private:
        YuvFormat format_ = YuvFormat::I420;
        QSize size_;

        const quint8* plane_[3] = { nullptr, nullptr, nullptr };
        int stride_[3] = { 0, 0, 0 };

        Q_DISABLE_COPY_MOVE(YuvView)
    };

    // When allow_hardware is false the H264 path skips the HW MF decoder and goes straight to
    // the software backend. Used by clients to recover after a PERMANENT_ERROR from the HW path.
    static std::unique_ptr<VideoDecoder> create(proto::video::Encoding encoding, bool allow_hardware = true);

    virtual ~VideoDecoder() = default;

    // Decodes |packet|. On success the result is available through frame() in the decoder's native
    // pixel format (I420 or NV12); the frame size comes from the packet format on key frames.
    virtual Result decode(const proto::video::Packet& packet) = 0;

    // Frame produced by the last successful decode(). The view stays valid only until the next
    // decode() call.
    const YuvView& frame() const { return frame_; }

    // Whether decoding runs on dedicated hardware (a GPU or codec block) rather than on the CPU.
    virtual bool isHardwareAccelerated() const { return false; }

protected:
    VideoDecoder() = default;

    // Frame size carried by |packet|: from the packet format on key frames, otherwise the size of
    // the previously decoded frame.
    QSize frameSize(const proto::video::Packet& packet) const;

    YuvView frame_;

private:
    Q_DISABLE_COPY_MOVE(VideoDecoder)
};

#endif // BASE_CODEC_VIDEO_DECODER_H
