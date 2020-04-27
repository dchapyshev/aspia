//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "codec/scale_reducer.h"

#include "base/logging.h"
#include "desktop/frame_simple.h"

#include <libyuv/scale_argb.h>

namespace codec {

namespace {

int div(int num, int div)
{
    return (num + div - 1) / div;
}

desktop::Size scaledSize(const desktop::Size& source_size, int scale_factor)
{
    return desktop::Size(div(source_size.width() * scale_factor, ScaleReducer::kDefScaleFactor),
                         div(source_size.height() * scale_factor, ScaleReducer::kDefScaleFactor));
}

desktop::Rect scaledRect(const desktop::Rect& source_rect, int scale_factor)
{
    int left = (source_rect.left() * scale_factor) / ScaleReducer::kDefScaleFactor;
    int top = (source_rect.top() * scale_factor) / ScaleReducer::kDefScaleFactor;
    int right = div(source_rect.right() * scale_factor, ScaleReducer::kDefScaleFactor);
    int bottom = div(source_rect.bottom() * scale_factor, ScaleReducer::kDefScaleFactor);

    return desktop::Rect::makeLTRB(left, top, right, bottom);
}

} // namespace

ScaleReducer::ScaleReducer() = default;

ScaleReducer::~ScaleReducer() = default;

const desktop::Frame* ScaleReducer::scaleFrame(const desktop::Frame* source_frame, int scale_factor)
{
    DCHECK(source_frame);
    DCHECK(!source_frame->constUpdatedRegion().isEmpty());
    DCHECK(source_frame->format() == desktop::PixelFormat::ARGB());

    if (scale_factor == kDefScaleFactor)
        return source_frame;

    if (scale_factor < kMinScaleFactor)
        scale_factor = kMinScaleFactor;

    if (scale_factor > kMaxScaleFactor)
        scale_factor = kMaxScaleFactor;

    if (last_frame_size_ != source_frame->size() || last_scale_factor_ != scale_factor)
    {
        last_frame_size_ = source_frame->size();
        last_scale_factor_ = scale_factor;
        scaled_frame_.reset();
    }

    if (!scaled_frame_)
    {
        desktop::Size size = scaledSize(source_frame->size(), scale_factor);

        scaled_frame_ = desktop::FrameSimple::create(size, source_frame->format());
        if (!scaled_frame_)
            return nullptr;
    }

    const desktop::Size& source_size = source_frame->size();
    const desktop::Size& scaled_size = scaled_frame_->size();

    desktop::Rect scaled_frame_rect = desktop::Rect::makeSize(scaled_size);
    desktop::Region* updated_region = scaled_frame_->updatedRegion();

    updated_region->clear();

    for (desktop::Region::Iterator it(source_frame->constUpdatedRegion()); !it.isAtEnd(); it.advance())
    {
        desktop::Rect scaled_rect = scaledRect(it.rect(), scale_factor);
        scaled_rect.intersectWith(scaled_frame_rect);

        if (libyuv::ARGBScaleClip(source_frame->frameData(),
                                  source_frame->stride(),
                                  source_size.width(),
                                  source_size.height(),
                                  scaled_frame_->frameData(),
                                  scaled_frame_->stride(),
                                  scaled_size.width(),
                                  scaled_size.height(),
                                  scaled_rect.x(),
                                  scaled_rect.y(),
                                  scaled_rect.width(),
                                  scaled_rect.height(),
                                  libyuv::kFilterBox) == -1)
        {
            LOG(LS_WARNING) << "libyuv::ARGBScaleClip failed";
        }

        updated_region->addRect(scaled_rect);
    }

    return scaled_frame_.get();
}

} // namespace codec
