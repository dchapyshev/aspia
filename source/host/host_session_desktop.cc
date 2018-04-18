//
// PROJECT:         Aspia
// FILE:            host/host_session_desktop.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_desktop.h"

#include <QDebug>
#include <QEvent>

#include "base/message_serialization.h"
#include "host/clipboard.h"
#include "host/input_injector.h"
#include "host/screen_updater.h"

namespace aspia {

namespace {

const quint32 kSupportedVideoEncodings =
    proto::desktop::VIDEO_ENCODING_ZLIB |
    proto::desktop::VIDEO_ENCODING_VP8 |
    proto::desktop::VIDEO_ENCODING_VP9;

const quint32 kSupportedFeatures =
    proto::desktop::FEATURE_CURSOR_SHAPE |
    proto::desktop::FEATURE_CLIPBOARD;

enum MessageId
{
    ScreenUpdateMessage,
    ClipboardEventMessage,
    ConfigRequestMessage
};

} // namespace

HostSessionDesktop::HostSessionDesktop(proto::auth::SessionType session_type,
                                       const QString& channel_id)
    : HostSession(channel_id),
      session_type_(session_type)
{
    switch (session_type_)
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
            break;

        default:
            qFatal("Invalid session type: %d", session_type_);
            break;
    }
}

void HostSessionDesktop::startSession()
{
    proto::desktop::HostToClient message;

    message.mutable_config_request()->set_video_encodings(kSupportedVideoEncodings);

    if (session_type_ == proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
        message.mutable_config_request()->set_features(kSupportedFeatures);
    else
        message.mutable_config_request()->set_features(0);

    emit writeMessage(ConfigRequestMessage, serializeMessage(message));
    emit readMessage();
}

void HostSessionDesktop::stopSession()
{
    delete screen_updater_;
    delete clipboard_;
    input_injector_.reset();
}

void HostSessionDesktop::customEvent(QEvent* event)
{
    switch (event->type())
    {
        case ScreenUpdater::UpdateEvent::kType:
        {
            ScreenUpdater::UpdateEvent* update_event =
                reinterpret_cast<ScreenUpdater::UpdateEvent*>(event);

            Q_ASSERT(update_event->video_packet || update_event->cursor_shape);

            proto::desktop::HostToClient message;
            message.set_allocated_video_packet(update_event->video_packet.release());
            message.set_allocated_cursor_shape(update_event->cursor_shape.release());

            emit writeMessage(ScreenUpdateMessage, serializeMessage(message));
        }
        break;

        case ScreenUpdater::ErrorEvent::kType:
            emit errorOccurred();
            break;
    }
}

void HostSessionDesktop::messageReceived(const QByteArray& buffer)
{
    proto::desktop::ClientToHost message;

    if (!parseMessage(buffer, message))
    {
        emit errorOccurred();
        return;
    }

    if (message.has_pointer_event())
        readPointerEvent(message.pointer_event());
    else if (message.has_key_event())
        readKeyEvent(message.key_event());
    else if (message.has_clipboard_event())
        readClipboardEvent(message.clipboard_event());
    else if (message.has_config())
        readConfig(message.config());
    else
    {
        qDebug("Unhandled message from client");
    }

    emit readMessage();
}

void HostSessionDesktop::messageWritten(int message_id)
{
    switch (message_id)
    {
        case ScreenUpdateMessage:
        {
            if (!screen_updater_.isNull())
                screen_updater_->update();
        }
        break;

        default:
            break;
    }
}

void HostSessionDesktop::clipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (session_type_ != proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    proto::desktop::HostToClient message;
    message.mutable_clipboard_event()->CopyFrom(event);

    emit writeMessage(ClipboardEventMessage, serializeMessage(message));
}

void HostSessionDesktop::readPointerEvent(const proto::desktop::PointerEvent& event)
{
    if (session_type_ != proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
    {
        qWarning("Attempt to inject pointer event to desktop view session");
        emit errorOccurred();
        return;
    }

    if (!input_injector_)
        input_injector_.reset(new InputInjector(this));

    input_injector_->injectPointerEvent(event);
}

void HostSessionDesktop::readKeyEvent(const proto::desktop::KeyEvent& event)
{
    if (session_type_ != proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
    {
        qWarning("Attempt to inject key event to desktop view session");
        emit errorOccurred();
        return;
    }

    if (!input_injector_)
        input_injector_.reset(new InputInjector(this));

    input_injector_->injectKeyEvent(event);
}

void HostSessionDesktop::readClipboardEvent(const proto::desktop::ClipboardEvent& clipboard_event)
{
    if (session_type_ != proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
    {
        qWarning("Attempt to inject clipboard event to desktop view session");
        emit errorOccurred();
        return;
    }

    if (!clipboard_)
    {
        qWarning("Attempt to inject clipboard event to session with the clipboard disabled");
        emit errorOccurred();
        return;
    }

    clipboard_->injectClipboardEvent(clipboard_event);
}

void HostSessionDesktop::readConfig(const proto::desktop::Config& config)
{
    delete screen_updater_;
    delete clipboard_;

    if (config.flags() & proto::desktop::Config::ENABLE_CLIPBOARD)
    {
        if (session_type_ != proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
        {
            qWarning("Attempt to enable clipboard in desktop view session");
            emit errorOccurred();
            return;
        }

        clipboard_ = new Clipboard(this);

        connect(clipboard_, &Clipboard::clipboardEvent,
                this, &HostSessionDesktop::clipboardEvent);
    }

    screen_updater_ = new ScreenUpdater(config, this);
}

} // namespace aspia
