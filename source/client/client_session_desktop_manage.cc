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

#include "client/ui/desktop_window.h"
#include "codec/cursor_decoder.h"
#include "protocol/message_serialization.h"

namespace aspia {

namespace {

constexpr char kMimeTypeTextUtf8[] = "text/plain; charset=UTF-8";

} // namespace

ClientSessionDesktopManage::ClientSessionDesktopManage(proto::Computer* computer, QObject* parent)
    : ClientSessionDesktopView(computer, parent)
{
    connect(desktop_window_, SIGNAL(sendKeyEvent(quint32, quint32)),
            this, SLOT(onSendKeyEvent(quint32, quint32)),
            Qt::DirectConnection);

    connect(desktop_window_, SIGNAL(sendPointerEvent(const QPoint&, quint32)),
            this, SLOT(onSendPointerEvent(const QPoint&, quint32)),
            Qt::DirectConnection);

    connect(desktop_window_, SIGNAL(sendClipboardEvent(const QString&)),
            this, SLOT(onSendClipboardEvent(const QString&)),
            Qt::DirectConnection);
}

void ClientSessionDesktopManage::readMessage(const QByteArray& buffer)
{
    proto::desktop::HostToClient message;

    if (!ParseMessage(buffer, message))
    {
        emit sessionError(tr("Session error: Invalid message from host."));
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
    if (!(config.flags() & proto::desktop::Config::ENABLE_CURSOR_SHAPE))
        cursor_decoder_.reset();

    if (!(config.flags() & proto::desktop::Config::ENABLE_CLIPBOARD))
    {
        last_clipboard_mime_type_.clear();
        last_clipboard_data_.clear();
    }

    proto::desktop::ClientToHost message;
    message.mutable_config()->CopyFrom(config);
    writeMessage(message);
}

void ClientSessionDesktopManage::onSendKeyEvent(quint32 usb_keycode, quint32 flags)
{
    proto::desktop::ClientToHost message;

    proto::desktop::KeyEvent* event = message.mutable_key_event();
    event->set_usb_keycode(usb_keycode);
    event->set_flags(flags);

    writeMessage(message);
}

void ClientSessionDesktopManage::onSendPointerEvent(const QPoint& pos, quint32 mask)
{
    proto::desktop::ClientToHost message;

    proto::desktop::PointerEvent* event = message.mutable_pointer_event();
    event->set_x(pos.x());
    event->set_y(pos.y());
    event->set_mask(mask);

    writeMessage(message);
}

void ClientSessionDesktopManage::onSendClipboardEvent(const QString& text)
{
    if (!(computer_->desktop_manage_session().flags() & proto::desktop::Config::ENABLE_CLIPBOARD))
        return;

    proto::desktop::ClientToHost message;
    message.mutable_clipboard_event()->set_mime_type(kMimeTypeTextUtf8);
    message.mutable_clipboard_event()->set_data(QString(text).replace("\r\n", "\n").toUtf8());

    if (message.clipboard_event().mime_type() == last_clipboard_mime_type_ &&
        message.clipboard_event().data() == last_clipboard_data_)
    {
        return;
    }

    writeMessage(message);
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

    if (clipboard_event.mime_type() != kMimeTypeTextUtf8)
        return;

    last_clipboard_mime_type_ = clipboard_event.mime_type();
    last_clipboard_data_ = clipboard_event.data();

    QString text = QString::fromUtf8(clipboard_event.data().c_str(),
                                     clipboard_event.data().size());
#if defined(Q_OS_WIN)
    text.replace("\n", "\r\n");
#endif

    desktop_window_->injectClipboard(text);
}

} // namespace aspia
