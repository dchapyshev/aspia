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

#ifndef BASE_CODEC_YUV_CONVERTER_H
#define BASE_CODEC_YUV_CONVERTER_H

#include <QList>
#include <QRect>

#include <memory>

#include "base/codec/video_decoder.h"

class Frame;

// Converts decoded YUV video frames (I420 / NV12) into the ARGB pixel format used for rendering and
// owns the ARGB output frame. Only the rectangles in the source frame's updated region are converted,
// so the rest of the output is preserved between frames.
class YuvConverter
{
public:
    enum class Result
    {
        FAILED,     // Conversion could not be completed (allocation failure or invalid rectangle).
        LAST_FRAME, // Converted into the existing frame; the buffer did not change.
        NEW_FRAME,  // A new frame buffer was allocated (first frame or size change).
    };

    YuvConverter();
    ~YuvConverter();

    // Converts |dirty_rects| of |src| into the owned ARGB frame, (re)allocating it when the frame
    // size changes.
    Result convert(const VideoDecoder::YuvView& src, const QList<QRect>& dirty_rects);

    const std::shared_ptr<Frame>& frame() const { return frame_; }

private:
    std::shared_ptr<Frame> frame_;

    Q_DISABLE_COPY_MOVE(YuvConverter)
};

#endif // BASE_CODEC_YUV_CONVERTER_H
