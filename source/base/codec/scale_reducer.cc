//
// SmartCafe Project
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

#include "base/codec/scale_reducer.h"

#include "base/logging.h"
#include "base/desktop/frame_simple.h"

#include <libyuv/scale_argb.h>

namespace base {

//--------------------------------------------------------------------------------------------------
ScaleReducer::ScaleReducer()
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ScaleReducer::~ScaleReducer()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
const Frame* ScaleReducer::scaleFrame(const Frame* source_frame, const QSize& target_size)
{
    DCHECK(source_frame);
    DCHECK(!source_frame->constUpdatedRegion().isEmpty());
    DCHECK(source_frame->format() == PixelFormat::ARGB());

    const QSize& source_size = source_frame->size();
    if (source_size.width() == 0 || source_size.height() == 0)
    {
        LOG(ERROR) << "Invalid source frame size:" << source_size;
        return nullptr;
    }

    if (source_size_ != source_size || target_size_ != target_size)
    {
        *const_cast<Frame*>(source_frame)->updatedRegion() += QRect(QPoint(0, 0), source_size);

        scale_x_ = static_cast<double>(target_size.width() * 100.0) /
            static_cast<double>(source_size.width());
        scale_y_ = static_cast<double>(target_size.height() * 100.0) /
            static_cast<double>(source_size.height());
        source_size_ = source_size;
        target_size_ = target_size;
        target_frame_.reset();

        LOG(INFO) << "Scale mode changed (source:" << source_size << "target:" << target_size
                  << "scale_x:" << scale_x_ << "scale_y:" << scale_y_ << ")";
    }

    if (source_size == target_size)
        return source_frame;

    QRect target_frame_rect(QPoint(0, 0), target_size);

    if (!target_frame_)
    {
        target_frame_ = FrameSimple::create(target_size, PixelFormat::ARGB());
        if (!target_frame_)
        {
            LOG(ERROR) << "Unable to create target frame";
            return nullptr;
        }

        *target_frame_->updatedRegion() += target_frame_rect;

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
        QRegion* updated_region = target_frame_->updatedRegion();
        *updated_region = QRegion();

        for (const auto& rect : source_frame->constUpdatedRegion())
        {
            QRect target_rect = scaledRect(rect);
            target_rect = target_rect.intersected(target_frame_rect);

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

            *updated_region += target_rect;
        }
    }

    return target_frame_.get();
}

//--------------------------------------------------------------------------------------------------
QRect ScaleReducer::scaledRect(const QRect& source_rect)
{
    int left = int(double(source_rect.left() * scale_x_) / 100.0);
    int top = int(double(source_rect.top() * scale_y_) / 100.0);
    int right = int(double(source_rect.right() * scale_x_) / 100.0);
    int bottom = int(double(source_rect.bottom() * scale_y_) / 100.0);

    return QRect(QPoint(left - 1, top - 1), QPoint(right + 2, bottom + 2));
}

} // namespace base
