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

#include "client/client_session_desktop_manage.h"

#include "client/ui/desktop_window.h"
#include "codec/cursor_decoder.h"
#include "common/desktop_session_constants.h"
#include "common/message_serialization.h"
#include "desktop_capture/mouse_cursor.h"

namespace aspia {

ClientSessionDesktopManage::ClientSessionDesktopManage(ConnectData* connect_data, QObject* parent)
    : ClientSessionDesktopView(connect_data, parent)
{
    connect(desktop_window_, &DesktopWindow::sendKeyEvent,
            this, &ClientSessionDesktopManage::onSendKeyEvent);

    connect(desktop_window_, &DesktopWindow::sendPointerEvent,
            this, &ClientSessionDesktopManage::onSendPointerEvent);

    connect(desktop_window_, &DesktopWindow::sendClipboardEvent,
            this, &ClientSessionDesktopManage::onSendClipboardEvent);

    connect(desktop_window_, &DesktopWindow::sendPowerControl,
            this, &ClientSessionDesktopManage::onPowerControl);
}

void ClientSessionDesktopManage::messageReceived(const QByteArray& buffer)
{
    incoming_message_.Clear();

    if (!parseMessage(buffer, incoming_message_))
    {
        emit errorOccurred(tr("Session error: Invalid message from host."));
        return;
    }

    if (incoming_message_.has_video_packet() || incoming_message_.has_cursor_shape())
    {
        if (incoming_message_.has_video_packet())
            readVideoPacket(incoming_message_.video_packet());

        if (incoming_message_.has_cursor_shape())
            readCursorShape(incoming_message_.cursor_shape());
    }
    else if (incoming_message_.has_clipboard_event())
    {
        readClipboardEvent(incoming_message_.clipboard_event());
    }
    else if (incoming_message_.has_config_request())
    {
        readConfigRequest(incoming_message_.config_request());
    }
    else if (incoming_message_.has_extension())
    {
        readExtension(incoming_message_.extension());
    }
    else
    {
        // Unknown messages are ignored.
        LOG(LS_WARNING) << "Unhandled message from host";
    }
}

void ClientSessionDesktopManage::onSendConfig(const proto::desktop::Config& config)
{
    if (!(config.flags() & proto::desktop::ENABLE_CURSOR_SHAPE))
        cursor_decoder_.reset();

    outgoing_message_.Clear();
    outgoing_message_.mutable_config()->CopyFrom(config);
    emit sendMessage(serializeMessage(outgoing_message_));
}

void ClientSessionDesktopManage::onSendKeyEvent(uint32_t usb_keycode, uint32_t flags)
{
    outgoing_message_.Clear();

    proto::desktop::KeyEvent* event = outgoing_message_.mutable_key_event();
    event->set_usb_keycode(usb_keycode);
    event->set_flags(flags);

    emit sendMessage(serializeMessage(outgoing_message_));
}

void ClientSessionDesktopManage::onSendPointerEvent(const QPoint& pos, uint32_t mask)
{
    outgoing_message_.Clear();

    proto::desktop::PointerEvent* event = outgoing_message_.mutable_pointer_event();
    event->set_x(pos.x());
    event->set_y(pos.y());
    event->set_mask(mask);

    emit sendMessage(serializeMessage(outgoing_message_));
}

void ClientSessionDesktopManage::onSendClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    uint32_t flags = connect_data_->desktop_config.flags();
    if (!(flags & proto::desktop::ENABLE_CLIPBOARD))
        return;

    outgoing_message_.Clear();
    outgoing_message_.mutable_clipboard_event()->CopyFrom(event);
    emit sendMessage(serializeMessage(outgoing_message_));
}

void ClientSessionDesktopManage::onPowerControl(proto::desktop::PowerControl::Action action)
{
    outgoing_message_.Clear();

    proto::desktop::Extension* extension = outgoing_message_.mutable_extension();

    proto::desktop::PowerControl power_control;
    power_control.set_action(action);

    extension->set_name(kPowerControlExtension);
    extension->set_data(power_control.SerializeAsString());

    emit sendMessage(serializeMessage(outgoing_message_));
}

void ClientSessionDesktopManage::readCursorShape(const proto::desktop::CursorShape& cursor_shape)
{
    uint32_t flags = connect_data_->desktop_config.flags();
    if (!(flags & proto::desktop::ENABLE_CURSOR_SHAPE))
        return;

    if (!cursor_decoder_)
        cursor_decoder_ = std::make_unique<CursorDecoder>();

    std::shared_ptr<MouseCursor> mouse_cursor = cursor_decoder_->decode(cursor_shape);
    if (!mouse_cursor)
        return;

    QImage image(mouse_cursor->data(),
                 mouse_cursor->size().width(),
                 mouse_cursor->size().height(),
                 mouse_cursor->stride(),
                 QImage::Format::Format_ARGB32);

    desktop_window_->injectCursor(
        QCursor(QPixmap::fromImage(image),
                mouse_cursor->hotSpot().x(),
                mouse_cursor->hotSpot().y()));
}

void ClientSessionDesktopManage::readClipboardEvent(
    const proto::desktop::ClipboardEvent& clipboard_event)
{
    uint32_t flags = connect_data_->desktop_config.flags();
    if (!(flags & proto::desktop::ENABLE_CLIPBOARD))
        return;

    desktop_window_->injectClipboard(clipboard_event);
}

} // namespace aspia
