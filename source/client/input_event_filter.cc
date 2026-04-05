//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/logging.h"

namespace client {

//--------------------------------------------------------------------------------------------------
InputEventFilter::InputEventFilter() = default;

//--------------------------------------------------------------------------------------------------
InputEventFilter::~InputEventFilter() = default;

//--------------------------------------------------------------------------------------------------
void InputEventFilter::setClipboardEnabled(bool enable)
{
    LOG(INFO) << "Clipboard enabled:" << enable;
    clipboard_enabled_ = enable;
}

//--------------------------------------------------------------------------------------------------
bool InputEventFilter::mouseEvent(const proto::input::MouseEvent& event)
{
    qint32 delta_x = std::abs(event.x() - last_pos_x_);
    qint32 delta_y = std::abs(event.y() - last_pos_y_);

    if (delta_x > 1 || delta_y > 1 || event.mask() != last_mask_)
    {
        static const quint32 kWheelMask =
            proto::input::MouseEvent::WHEEL_DOWN | proto::input::MouseEvent::WHEEL_UP;

        last_pos_x_ = event.x();
        last_pos_y_ = event.y();
        last_mask_ = event.mask() & ~kWheelMask;

        ++send_mouse_count_;
        return true;
    }
    else
    {
        ++drop_mouse_count_;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool InputEventFilter::keyEvent(const proto::input::KeyEvent& event)
{
    ++send_key_count_;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool InputEventFilter::textEvent(const proto::input::TextEvent& event)
{
    ++send_text_count_;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool InputEventFilter::readClipboardEvent(const proto::clipboard::Event& event)
{
    if (!clipboard_enabled_)
        return false;

    ++read_clipboard_count_;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool InputEventFilter::sendClipboardEvent(const proto::clipboard::Event& event)
{
    if (!clipboard_enabled_)
        return false;

    ++send_clipboard_count_;
    return true;
}

} // namespace client
