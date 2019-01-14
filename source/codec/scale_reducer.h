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

#ifndef CODEC__SCALE_REDUCER_H
#define CODEC__SCALE_REDUCER_H

#include <memory>

#include "base/macros_magic.h"
#include "desktop/screen_settings_tracker.h"

namespace desktop {
class Frame;
} // namespace aspia

namespace codec {

class ScaleReducer
{
public:
    ~ScaleReducer() = default;

    static ScaleReducer* create(int scale_factor);

    const desktop::Frame* scaleFrame(const desktop::Frame* source_frame);

protected:
    explicit ScaleReducer(int scale_factor);

private:
    const int scale_factor_;
    std::unique_ptr<desktop::Frame> scaled_frame_;
    desktop::ScreenSettingsTracker screen_settings_tracker_;

    DISALLOW_COPY_AND_ASSIGN(ScaleReducer);
};

} // namespace codec

#endif // CODEC__SCALE_REDUCER_H
