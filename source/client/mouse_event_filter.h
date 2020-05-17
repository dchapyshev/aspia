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

#ifndef CLIENT__MOUSE_EVENT_FILTER_H
#define CLIENT__MOUSE_EVENT_FILTER_H

#include "proto/desktop.pb.h"

#include <optional>

namespace client {

class MouseEventFilter
{
public:
    MouseEventFilter();
    ~MouseEventFilter();

    std::optional<proto::MouseEvent> mouseEvent(const proto::MouseEvent& event);

    int sendCount() const { return send_count_; }
    int dropCount() const { return drop_count_; }

private:
    int32_t last_pos_x_ = 0;
    int32_t last_pos_y_ = 0;
    uint32_t last_mask_ = 0;

    int send_count_ = 0;
    int drop_count_ = 0;
};

} // namespace client

#endif // CLIENT__MOUSE_EVENT_FILTER_H
