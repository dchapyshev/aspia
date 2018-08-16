//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_CODEC__SCALE_REDUCER_H_
#define ASPIA_CODEC__SCALE_REDUCER_H_

#include <memory>

#include "base/macros_magic.h"

namespace aspia {

class DesktopFrame;

class ScaleReducer
{
public:
    ~ScaleReducer() = default;

    static ScaleReducer* create(int scale_factor);

    const DesktopFrame* scaleFrame(const DesktopFrame* source_frame);

protected:
    ScaleReducer(int scale_factor);

private:
    const int scale_factor_;
    std::unique_ptr<DesktopFrame> scaled_frame_;

    DISALLOW_COPY_AND_ASSIGN(ScaleReducer);
};

} // namespace aspia

#endif // ASPIA_CODEC__SCALE_REDUCER_H_
