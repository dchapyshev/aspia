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

#include "base/codec/yuv_converter.h"

#include <libyuv/convert_argb.h>

#include "base/desktop/frame.h"
#include "base/desktop/frame_aligned.h"

namespace {

// Row alignment of the ARGB output buffer, matching the value used elsewhere in the pipeline.
const size_t kArgbAlignment = 32;

} // namespace

//--------------------------------------------------------------------------------------------------
YuvConverter::YuvConverter() = default;

//--------------------------------------------------------------------------------------------------
YuvConverter::~YuvConverter() = default;

//--------------------------------------------------------------------------------------------------
YuvConverter::Result YuvConverter::convert(
    const VideoDecoder::YuvView& src, const QList<QRect>& dirty_rects)
{
    Result result = Result::LAST_FRAME;

    if (!frame_.isValid() || frame_.size() != src.size())
    {
        std::unique_ptr<FrameAligned> aligned = FrameAligned::create(src.size(), kArgbAlignment);
        if (!aligned)
            return Result::FAILED;
        frame_ = SharedFrame(std::move(aligned));
        result = Result::NEW_FRAME;
    }

    // The GUI thread paints this same buffer; the write access holds the lock while converting so a
    // paint never sees a half-written frame.
    const SharedFrame::WriteAccess access = frame_.write();
    Frame& dst = access.get();

    const QRect frame_rect(QPoint(0, 0), src.size());
    const int y_stride = src.planeStride(0);

    if (src.format() == VideoDecoder::YuvFormat::NV12)
    {
        const int uv_stride = src.planeStride(1);

        for (const QRect& rect : dirty_rects)
        {
            if (!frame_rect.contains(rect))
                return Result::FAILED;

            const int y_offset = rect.y() * y_stride + rect.x();
            const int uv_offset = (rect.y() / 2) * uv_stride + (rect.x() & ~1);

            libyuv::NV12ToARGB(src.planeData(0) + y_offset, y_stride,
                               src.planeData(1) + uv_offset, uv_stride,
                               dst.frameDataAtPos(rect.topLeft()), dst.stride(),
                               rect.width(), rect.height());
        }
    }
    else
    {
        const int u_stride = src.planeStride(1);
        const int v_stride = src.planeStride(2);

        for (const QRect& rect : dirty_rects)
        {
            if (!frame_rect.contains(rect))
                return Result::FAILED;

            const int y_offset = rect.y() * y_stride + rect.x();
            const int u_offset = (rect.y() / 2) * u_stride + (rect.x() / 2);
            const int v_offset = (rect.y() / 2) * v_stride + (rect.x() / 2);

            libyuv::I420ToARGB(src.planeData(0) + y_offset, y_stride,
                               src.planeData(1) + u_offset, u_stride,
                               src.planeData(2) + v_offset, v_stride,
                               dst.frameDataAtPos(rect.topLeft()), dst.stride(),
                               rect.width(), rect.height());
        }
    }

    return result;
}
