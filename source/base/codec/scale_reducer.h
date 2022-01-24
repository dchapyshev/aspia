//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__CODEC__SCALE_REDUCER_H
#define BASE__CODEC__SCALE_REDUCER_H

#include "base/macros_magic.h"
#include "base/desktop/geometry.h"

#include <memory>

namespace base {

class Frame;

class ScaleReducer
{
public:
    ScaleReducer();
    ~ScaleReducer();

    const Frame* scaleFrame(const Frame* source_frame, const Size& target_size);

    double scaleFactorX() const { return scale_x_; }
    double scaleFactorY() const { return scale_y_; }

private:
    Rect scaledRect(const Rect& source_rect);

    std::unique_ptr<Frame> target_frame_;
    Size source_size_;
    Size target_size_;
    double scale_x_ = 0;
    double scale_y_ = 0;

    DISALLOW_COPY_AND_ASSIGN(ScaleReducer);
};

} // namespace base

#endif // BASE__CODEC__SCALE_REDUCER_H
