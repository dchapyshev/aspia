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

#include "client/mouse_event_filter.h"

namespace client {

MouseEventFilter::MouseEventFilter() = default;

MouseEventFilter::~MouseEventFilter() = default;

std::optional<proto::MouseEvent> MouseEventFilter::mouseEvent(const proto::MouseEvent& event)
{
    int32_t delta_x = std::abs(event.x() - last_pos_x_);
    int32_t delta_y = std::abs(event.y() - last_pos_y_);

    if (delta_x > 1 || delta_y > 1 || event.mask() != last_mask_)
    {
        last_pos_x_ = event.x();
        last_pos_y_ = event.y();
        last_mask_ = event.mask();

        ++send_count_;
        return event;
    }

    ++drop_count_;
    return std::nullopt;
}

} // namespace client
