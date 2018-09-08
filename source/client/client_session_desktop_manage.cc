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

#include "base/message_serialization.h"
#include "client/ui/desktop_window.h"
#include "codec/cursor_decoder.h"
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
}

void ClientSessionDesktopManage::messageReceived(const QByteArray& buffer)
{
    message_.Clear();

    if (!parseMessage(buffer, message_))
    {
        emit errorOccurred(tr("Session error: Invalid message from host."));
        return;
    }

    if (message_.has_video_packet() || message_.has_cursor_shape())
    {
        if (message_.has_video_packet())
            readVideoPacket(message_.video_packet());

        if (message_.has_cursor_shape())
            readCursorShape(message_.cursor_shape());
    }
    else if (message_.has_clipboard_event())
    {
        readClipboardEvent(message_.clipboard_event());
    }
    else if (message_.has_config_request())
    {
        readConfigRequest(message_.config_request());
    }
    else if (message_.has_screen_list())
    {
        readScreenList(message_.screen_list());
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

    proto::desktop::ClientToHost message;
    message.mutable_config()->CopyFrom(config);
    emit sendMessage(serializeMessage(message));
}

void ClientSessionDesktopManage::onSendKeyEvent(uint32_t usb_keycode, uint32_t flags)
{
    proto::desktop::ClientToHost message;

    proto::desktop::KeyEvent* event = message.mutable_key_event();
    event->set_usb_keycode(usb_keycode);
    event->set_flags(flags);

    emit sendMessage(serializeMessage(message));
}

void ClientSessionDesktopManage::onSendPointerEvent(const DesktopPoint& pos, uint32_t mask)
{
    proto::desktop::ClientToHost message;

    proto::desktop::PointerEvent* event = message.mutable_pointer_event();
    event->set_x(pos.x());
    event->set_y(pos.y());
    event->set_mask(mask);

    emit sendMessage(serializeMessage(message));
}

void ClientSessionDesktopManage::onSendClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    uint32_t flags = connect_data_->desktopConfig().flags();
    if (!(flags & proto::desktop::ENABLE_CLIPBOARD))
        return;

    proto::desktop::ClientToHost message;
    message.mutable_clipboard_event()->CopyFrom(event);

    emit sendMessage(serializeMessage(message));
}

void ClientSessionDesktopManage::readCursorShape(const proto::desktop::CursorShape& cursor_shape)
{
    uint32_t flags = connect_data_->desktopConfig().flags();
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
    uint32_t flags = connect_data_->desktopConfig().flags();
    if (!(flags & proto::desktop::ENABLE_CLIPBOARD))
        return;

    desktop_window_->injectClipboard(clipboard_event);
}

} // namespace aspia
