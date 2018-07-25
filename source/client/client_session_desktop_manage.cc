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

namespace aspia {

namespace {

const quint32 kSupportedVideoEncodings =
    proto::desktop::VIDEO_ENCODING_ZLIB |
    proto::desktop::VIDEO_ENCODING_VP8 |
    proto::desktop::VIDEO_ENCODING_VP9;

const quint32 kSupportedFeatures =
    proto::desktop::FEATURE_CURSOR_SHAPE |
    proto::desktop::FEATURE_CLIPBOARD;

} // namespace

ClientSessionDesktopManage::ClientSessionDesktopManage(ConnectData* connect_data,
                                                       QObject* parent)
    : ClientSessionDesktopView(connect_data, parent)
{
    connect(desktop_window_, &DesktopWindow::sendKeyEvent,
            this, &ClientSessionDesktopManage::onSendKeyEvent);

    connect(desktop_window_, &DesktopWindow::sendPointerEvent,
            this, &ClientSessionDesktopManage::onSendPointerEvent);

    connect(desktop_window_, &DesktopWindow::sendClipboardEvent,
            this, &ClientSessionDesktopManage::onSendClipboardEvent);
}

// static
quint32 ClientSessionDesktopManage::supportedVideoEncodings()
{
    return kSupportedVideoEncodings;
}

// static
quint32 ClientSessionDesktopManage::supportedFeatures()
{
    return kSupportedFeatures;
}

void ClientSessionDesktopManage::messageReceived(const QByteArray& buffer)
{
    proto::desktop::HostToClient message;

    if (!parseMessage(buffer, message))
    {
        emit errorOccurred(tr("Session error: Invalid message from host."));
        return;
    }

    if (message.has_video_packet() || message.has_cursor_shape())
    {
        if (message.has_video_packet())
            readVideoPacket(message.video_packet());

        if (message.has_cursor_shape())
            readCursorShape(message.cursor_shape());
    }
    else if (message.has_clipboard_event())
    {
        readClipboardEvent(message.clipboard_event());
    }
    else if (message.has_config_request())
    {
        readConfigRequest(message.config_request());
    }
    else
    {
        // Unknown messages are ignored.
        qWarning("Unhandled message from host");
    }
}

void ClientSessionDesktopManage::onSendConfig(const proto::desktop::Config& config)
{
    if (!(config.features() & proto::desktop::FEATURE_CURSOR_SHAPE))
        cursor_decoder_.reset();

    proto::desktop::ClientToHost message;
    message.mutable_config()->CopyFrom(config);
    emit sendMessage(serializeMessage(message));
}

void ClientSessionDesktopManage::onSendKeyEvent(quint32 usb_keycode, quint32 flags)
{
    proto::desktop::ClientToHost message;

    proto::desktop::KeyEvent* event = message.mutable_key_event();
    event->set_usb_keycode(usb_keycode);
    event->set_flags(flags);

    emit sendMessage(serializeMessage(message));
}

void ClientSessionDesktopManage::onSendPointerEvent(const QPoint& pos, quint32 mask)
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
    quint32 features = connect_data_->desktopConfig().features();
    if (!(features & proto::desktop::FEATURE_CLIPBOARD))
        return;

    proto::desktop::ClientToHost message;
    message.mutable_clipboard_event()->CopyFrom(event);

    emit sendMessage(serializeMessage(message));
}

void ClientSessionDesktopManage::readConfigRequest(
    const proto::desktop::ConfigRequest& config_request)
{
    proto::desktop::Config config = connect_data_->desktopConfig();

    desktop_window_->setSupportedVideoEncodings(config_request.video_encodings());
    desktop_window_->setSupportedFeatures(config_request.features());

    // If current video encoding not supported.
    if (!(config_request.video_encodings() & config.video_encoding()))
    {
        if (!(config_request.video_encodings() & kSupportedVideoEncodings))
        {
            emit errorOccurred(tr("Session error: There are no supported video encodings."));
            return;
        }
        else
        {
            if (!desktop_window_->requireConfigChange(&config))
            {
                emit errorOccurred(tr("Session error: Canceled by the user."));
                return;
            }
        }
    }

    connect_data_->setDesktopConfig(config);
    onSendConfig(config);
}

void ClientSessionDesktopManage::readCursorShape(const proto::desktop::CursorShape& cursor_shape)
{
    quint32 features = connect_data_->desktopConfig().features();
    if (!(features & proto::desktop::FEATURE_CURSOR_SHAPE))
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
    quint32 features = connect_data_->desktopConfig().features();
    if (!(features & proto::desktop::FEATURE_CLIPBOARD))
        return;

    desktop_window_->injectClipboard(clipboard_event);
}

} // namespace aspia
