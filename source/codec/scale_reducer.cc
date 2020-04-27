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
#include <libyuv/scale_row.h>

namespace codec {

namespace {

int div(int num, int div)
{
    return (num + div - 1) / div;
}

desktop::Size scaledSize(const desktop::Size& source, int scale_factor)
{
    return desktop::Size(div(source.width() * scale_factor, ScaleReducer::kDefScaleFactor),
                         div(source.height() * scale_factor, ScaleReducer::kDefScaleFactor));
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

const desktop::Frame* ScaleReducer::scaleFrame(const desktop::Frame* source_frame,
                                               int scale_factor)
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

    const desktop::Size& source_size = source_frame->size();

    if (last_frame_size_ != source_size || last_scale_factor_ != scale_factor)
    {
        last_scale_factor_ = scale_factor;
        target_frame_.reset();
    }

    if (!target_frame_)
    {
        desktop::Size target_size = scaledSize(source_size, scale_factor);

        target_frame_ = desktop::FrameSimple::create(target_size, source_frame->format());
        if (!target_frame_)
            return nullptr;

        target_frame_->updatedRegion()->addRect(desktop::Rect::makeSize(target_size));

        libyuv::ARGBScale(source_frame->frameData(),
                          source_frame->stride(),
                          source_size.width(),
                          source_size.height(),
                          target_frame_->frameData(),
                          target_frame_->stride(),
                          target_size.width(),
                          target_size.height(),
                          libyuv::kFilterBox);
    }
    else
    {
        desktop::Region* updated_region = target_frame_->updatedRegion();
        const desktop::Size& target_size = target_frame_->size();
        desktop::Rect scaled_frame_rect = desktop::Rect::makeSize(target_size);

        updated_region->clear();

        for (desktop::Region::Iterator it(source_frame->constUpdatedRegion());
             !it.isAtEnd(); it.advance())
        {
            desktop::Rect source_rect = it.rect();
            desktop::Rect target_rect = scaledRect(source_rect, scale_factor);

            target_rect.intersectWith(scaled_frame_rect);

            libyuv::ARGBScaleClip(source_frame->frameData(),
                                  source_frame->stride(),
                                  source_size.width(),
                                  source_size.height(),
                                  target_frame_->frameData(),
                                  target_frame_->stride(),
                                  target_size.width(),
                                  target_size.height(),
                                  target_rect.x(),
                                  target_rect.y(),
                                  target_rect.width(),
                                  target_rect.height(),
                                  libyuv::kFilterBox);

            updated_region->addRect(target_rect);
        }
    }

    return target_frame_.get();
}

} // namespace codec
