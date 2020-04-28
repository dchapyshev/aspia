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

#ifndef CODEC__SCALE_REDUCER_H
#define CODEC__SCALE_REDUCER_H

#include "base/macros_magic.h"
#include "desktop/geometry.h"

#include <memory>

namespace desktop {
class Frame;
} // namespace aspia

namespace codec {

class ScaleReducer
{
public:
    ScaleReducer();
    ~ScaleReducer();

    static const int kMinScaleFactor = 30;
    static const int kMaxScaleFactor = 100;
    static const int kDefScaleFactor = 100;

    void setScaleFactor(int scale_factor);
    int scaleFactor() const;

    const desktop::Frame* scaleFrame(const desktop::Frame* source_frame);

private:
    std::unique_ptr<desktop::Frame> target_frame_;
    int scale_factor_ = kDefScaleFactor;
    desktop::Size last_frame_size_;

    DISALLOW_COPY_AND_ASSIGN(ScaleReducer);
};

} // namespace codec

#endif // CODEC__SCALE_REDUCER_H
