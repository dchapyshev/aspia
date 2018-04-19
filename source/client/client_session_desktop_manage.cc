//
// PROJECT:         Aspia
// FILE:            client/client_session_desktop_manage.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_desktop_manage.h"

#include <QDebug>
#include <QCursor>
#include <QPixmap>

#include "base/message_serialization.h"
#include "client/ui/desktop_window.h"
#include "codec/cursor_decoder.h"

namespace aspia {

namespace {

enum MessageId
{
    ConfigMessageId,
    KeyEventMessageId,
    PointerEventMessageId,
    ClipboardEventMessageId
};

} // namespace

ClientSessionDesktopManage::ClientSessionDesktopManage(proto::Computer* computer, QObject* parent)
    : ClientSessionDesktopView(computer, parent)
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

    emit readMessage();
}

void ClientSessionDesktopManage::messageWritten(int /* message_id */)
{
    // Nothing
}

void ClientSessionDesktopManage::onSendConfig(const proto::desktop::Config& config)
{
    if (!(config.flags() & proto::desktop::Config::ENABLE_CURSOR_SHAPE))
        cursor_decoder_.reset();

    proto::desktop::ClientToHost message;
    message.mutable_config()->CopyFrom(config);
    emit writeMessage(ConfigMessageId, serializeMessage(message));
}

void ClientSessionDesktopManage::onSendKeyEvent(quint32 usb_keycode, quint32 flags)
{
    proto::desktop::ClientToHost message;

    proto::desktop::KeyEvent* event = message.mutable_key_event();
    event->set_usb_keycode(usb_keycode);
    event->set_flags(flags);

    emit writeMessage(KeyEventMessageId, serializeMessage(message));
}

void ClientSessionDesktopManage::onSendPointerEvent(const QPoint& pos, quint32 mask)
{
    proto::desktop::ClientToHost message;

    proto::desktop::PointerEvent* event = message.mutable_pointer_event();
    event->set_x(pos.x());
    event->set_y(pos.y());
    event->set_mask(mask);

    emit writeMessage(PointerEventMessageId, serializeMessage(message));
}

void ClientSessionDesktopManage::onSendClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (!(computer_->desktop_manage_session().flags() & proto::desktop::Config::ENABLE_CLIPBOARD))
        return;

    proto::desktop::ClientToHost message;
    message.mutable_clipboard_event()->CopyFrom(event);

    emit writeMessage(ClipboardEventMessageId, serializeMessage(message));
}

void ClientSessionDesktopManage::readConfigRequest(
    const proto::desktop::ConfigRequest& config_request)
{
    onSendConfig(computer_->desktop_manage_session());
}

void ClientSessionDesktopManage::readCursorShape(const proto::desktop::CursorShape& cursor_shape)
{
    if (!(computer_->desktop_manage_session().flags() & proto::desktop::Config::ENABLE_CURSOR_SHAPE))
        return;

    if (!cursor_decoder_)
        cursor_decoder_ = std::make_unique<CursorDecoder>();

    std::shared_ptr<MouseCursor> mouse_cursor =
        cursor_decoder_->Decode(cursor_shape);

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
    if (!(computer_->desktop_manage_session().flags() & proto::desktop::Config::ENABLE_CLIPBOARD))
        return;

    desktop_window_->injectClipboard(clipboard_event);
}

} // namespace aspia
