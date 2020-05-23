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

#include "client/input_event_filter.h"

namespace client {

InputEventFilter::InputEventFilter()
{
    // Nothing
}

InputEventFilter::~InputEventFilter() = default;

void InputEventFilter::setSessionType(proto::SessionType session_type)
{
    session_type_ = session_type;
}

void InputEventFilter::setClipboardEnabled(bool enable)
{
    clipboard_enabled_ = enable;
}

std::vector<proto::MouseEvent> InputEventFilter::mouseEvents(
    const std::vector<proto::MouseEvent>& events)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return std::vector<proto::MouseEvent>();

    std::vector<proto::MouseEvent> out_events;

    for (const auto& event : events)
    {
        int32_t delta_x = std::abs(event.x() - last_pos_x_);
        int32_t delta_y = std::abs(event.y() - last_pos_y_);

        if (delta_x > 1 || delta_y > 1 || event.mask() != last_mask_)
        {
            last_pos_x_ = event.x();
            last_pos_y_ = event.y();
            last_mask_ = event.mask();

            out_events.push_back(event);
        }
        else
        {
            ++drop_mouse_count_;
        }
    }

    size_t events_count = out_events.size();
    if (events_count > 0)
    {
        ++send_mouse_count_;

        if (events_count > 1)
            glue_mouse_count_ += static_cast<int>(events_count) - 1;
    }

    return out_events;
}

std::vector<proto::KeyEvent> InputEventFilter::keyEvents(
    const std::vector<proto::KeyEvent>& events)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return std::vector<proto::KeyEvent>();

    if (events.size() > 1)
        glue_key_count_ += static_cast<int>(events.size()) - 1;

    ++send_key_count_;
    return events;
}

std::optional<proto::ClipboardEvent> InputEventFilter::readClipboardEvent(
    const proto::ClipboardEvent& event)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return std::nullopt;

    if (!clipboard_enabled_)
        return std::nullopt;

    ++read_clipboard_count_;
    return event;
}

std::optional<proto::ClipboardEvent> InputEventFilter::sendClipboardEvent(
    const proto::ClipboardEvent& event)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return std::nullopt;

    if (!clipboard_enabled_)
        return std::nullopt;

    ++send_clipboard_count_;
    return event;
}

} // namespace client
