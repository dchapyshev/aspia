//
// Aspia Project
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

#include "client/input_event_filter.h"

#include "base/logging.h"

namespace client {

//--------------------------------------------------------------------------------------------------
InputEventFilter::InputEventFilter() = default;

//--------------------------------------------------------------------------------------------------
InputEventFilter::~InputEventFilter() = default;

//--------------------------------------------------------------------------------------------------
void InputEventFilter::setSessionType(proto::SessionType session_type)
{
    LOG(LS_INFO) << "Session type changed: " << session_type;
    session_type_ = session_type;
}

//--------------------------------------------------------------------------------------------------
void InputEventFilter::setClipboardEnabled(bool enable)
{
    LOG(LS_INFO) << "Clipboard enabled: " << enable;
    clipboard_enabled_ = enable;
}

//--------------------------------------------------------------------------------------------------
void InputEventFilter::setNetworkOverflow(bool enable)
{
    network_overflow_ = enable;
}

//--------------------------------------------------------------------------------------------------
std::optional<proto::MouseEvent> InputEventFilter::mouseEvent(const proto::MouseEvent& event)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return std::nullopt;

    if (network_overflow_)
        return std::nullopt;

    qint32 delta_x = std::abs(event.x() - last_pos_x_);
    qint32 delta_y = std::abs(event.y() - last_pos_y_);

    if (delta_x > 1 || delta_y > 1 || event.mask() != last_mask_)
    {
        static const quint32 kWheelMask =
            proto::MouseEvent::WHEEL_DOWN | proto::MouseEvent::WHEEL_UP;

        last_pos_x_ = event.x();
        last_pos_y_ = event.y();
        last_mask_ = event.mask() & ~kWheelMask;

        ++send_mouse_count_;
        return event;
    }
    else
    {
        ++drop_mouse_count_;
    }

    return std::nullopt;
}

//--------------------------------------------------------------------------------------------------
std::optional<proto::KeyEvent> InputEventFilter::keyEvent(const proto::KeyEvent& event)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return std::nullopt;

    if (network_overflow_)
        return std::nullopt;

    ++send_key_count_;
    return event;
}

//--------------------------------------------------------------------------------------------------
std::optional<proto::TextEvent> InputEventFilter::textEvent(const proto::TextEvent& event)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return std::nullopt;

    if (network_overflow_)
        return std::nullopt;

    ++send_text_count_;
    return event;
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
std::optional<proto::ClipboardEvent> InputEventFilter::sendClipboardEvent(
    const proto::ClipboardEvent& event)
{
    if (session_type_ != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return std::nullopt;

    if (network_overflow_)
        return std::nullopt;

    if (!clipboard_enabled_)
        return std::nullopt;

    ++send_clipboard_count_;
    return event;
}

} // namespace client
