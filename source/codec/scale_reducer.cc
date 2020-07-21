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
#include "base/desktop/frame_simple.h"

#include <libyuv/scale_argb.h>

namespace codec {

ScaleReducer::ScaleReducer() = default;

ScaleReducer::~ScaleReducer() = default;

const base::Frame* ScaleReducer::scaleFrame(
    const base::Frame* source_frame, const base::Size& target_size)
{
    DCHECK(source_frame);
    DCHECK(!source_frame->constUpdatedRegion().isEmpty());
    DCHECK(source_frame->format() == base::PixelFormat::ARGB());

    const base::Size& source_size = source_frame->size();

    if (source_size_ != source_size || target_size_ != target_size)
    {
        const_cast<base::Frame*>(source_frame)->updatedRegion()->addRect(
            base::Rect::makeSize(source_size));

        scale_x_ = double(target_size.width() * 100.0) / double(source_size.width());
        scale_y_ = double(target_size.height() * 100.0) / double(source_size.height());
        source_size_ = source_size;
        target_size_ = target_size;
        target_frame_.reset();

        LOG(LS_INFO) << "Scale mode changed (dpi:" << source_frame->dpi()
                     << " source:" << source_size << " target:" << target_size
                     << " scale_x:" << scale_x_ << " scale_y:" << scale_y_ << ")";
    }

    if (source_size == target_size)
        return source_frame;

    base::Rect target_frame_rect = base::Rect::makeSize(target_size);

    if (!target_frame_)
    {
        target_frame_ = base::FrameSimple::create(target_size, source_frame->format());
        if (!target_frame_)
            return nullptr;

        target_frame_->updatedRegion()->addRect(target_frame_rect);

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
        base::Region* updated_region = target_frame_->updatedRegion();
        updated_region->clear();

        for (base::Region::Iterator it(source_frame->constUpdatedRegion());
             !it.isAtEnd(); it.advance())
        {
            base::Rect target_rect = scaledRect(it.rect());
            target_rect.intersectWith(target_frame_rect);

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

base::Rect ScaleReducer::scaledRect(const base::Rect& source_rect)
{
    int left = int(double(source_rect.left() * scale_x_) / 100.0);
    int top = int(double(source_rect.top() * scale_y_) / 100.0);
    int right = int(double(source_rect.right() * scale_x_) / 100.0);
    int bottom = int(double(source_rect.bottom() * scale_y_) / 100.0);

    return base::Rect::makeLTRB(left - 1, top - 1, right + 2, bottom + 2);
}

} // namespace codec
