//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/client_desktop.h"

#include <QCursor>
#include <QImage>
#include <QPixmap>

#include "base/logging.h"
#include "codec/cursor_decoder.h"
#include "codec/video_decoder.h"
#include "codec/video_util.h"
#include "common/desktop_session_constants.h"
#include "desktop/mouse_cursor.h"

namespace client {

ClientDesktop::ClientDesktop(const ConnectData& connect_data, Delegate* delegate, QObject* parent)
    : Client(connect_data, parent),
      delegate_(delegate)
{
    // Nothing
}

ClientDesktop::~ClientDesktop() = default;

void ClientDesktop::messageReceived(const QByteArray& buffer)
{
    incoming_message_.Clear();

    if (!incoming_message_.ParseFromArray(buffer.constData(), buffer.size()))
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

void ClientDesktop::sendKeyEvent(uint32_t usb_keycode, uint32_t flags)
{
    if (connectData().session_type != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    outgoing_message_.Clear();

    proto::desktop::KeyEvent* event = outgoing_message_.mutable_key_event();
    event->set_usb_keycode(usb_keycode);
    event->set_flags(flags);

    sendMessage(outgoing_message_);
}

void ClientDesktop::sendPointerEvent(const QPoint& pos, uint32_t mask)
{
    if (connectData().session_type != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    outgoing_message_.Clear();

    proto::desktop::PointerEvent* event = outgoing_message_.mutable_pointer_event();
    event->set_x(pos.x());
    event->set_y(pos.y());
    event->set_mask(mask);

    sendMessage(outgoing_message_);
}

void ClientDesktop::sendClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    const ConnectData& connect_data = connectData();

    if (connect_data.session_type != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    uint32_t flags = connect_data.desktop_config.flags();
    if (!(flags & proto::desktop::ENABLE_CLIPBOARD))
        return;

    outgoing_message_.Clear();
    outgoing_message_.mutable_clipboard_event()->CopyFrom(event);
    sendMessage(outgoing_message_);
}

void ClientDesktop::sendPowerControl(proto::desktop::PowerControl::Action action)
{
    if (connectData().session_type != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    outgoing_message_.Clear();

    proto::desktop::Extension* extension = outgoing_message_.mutable_extension();

    proto::desktop::PowerControl power_control;
    power_control.set_action(action);

    extension->set_name(common::kPowerControlExtension);
    extension->set_data(power_control.SerializeAsString());

    sendMessage(outgoing_message_);
}

void ClientDesktop::sendConfig(const proto::desktop::Config& config)
{
    if (!(config.flags() & proto::desktop::ENABLE_CURSOR_SHAPE))
        cursor_decoder_.reset();

    outgoing_message_.Clear();
    outgoing_message_.mutable_config()->CopyFrom(config);
    sendMessage(outgoing_message_);
}

void ClientDesktop::sendScreen(const proto::desktop::Screen& screen)
{
    outgoing_message_.Clear();

    proto::desktop::Extension* extension = outgoing_message_.mutable_extension();

    extension->set_name(common::kSelectScreenExtension);
    extension->set_data(screen.SerializeAsString());

    sendMessage(outgoing_message_);
}

void ClientDesktop::sendRemoteUpdate()
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_extension()->set_name(common::kRemoteUpdateExtension);
    sendMessage(outgoing_message_);
}

void ClientDesktop::sendSysInfoRequest()
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_extension()->set_name(common::kSystemInfoExtension);
    sendMessage(outgoing_message_);
}

void ClientDesktop::readConfigRequest(
    const proto::desktop::ConfigRequest& /* config_request */)
{
    sendConfig(connectData().desktop_config);
}

void ClientDesktop::readVideoPacket(const proto::desktop::VideoPacket& packet)
{
    if (video_encoding_ != packet.encoding())
    {
        video_decoder_ = codec::VideoDecoder::create(packet.encoding());
        video_encoding_ = packet.encoding();
    }

    if (!video_decoder_)
    {
        emit errorOccurred(tr("Session error: Video decoder not initialized."));
        return;
    }

    if (packet.has_format())
    {
        QRect screen_rect = codec::VideoUtil::fromVideoRect(packet.format().screen_rect());

        static const int kMaxValue = std::numeric_limits<uint16_t>::max();
        static const int kMinValue = -std::numeric_limits<uint16_t>::max();

        if (screen_rect.width()  <= 0 || screen_rect.width()  >= kMaxValue ||
            screen_rect.height() <= 0 || screen_rect.height() >= kMaxValue)
        {
            emit errorOccurred(tr("Session error: Wrong video frame size."));
            return;
        }

        if (screen_rect.x() < kMinValue || screen_rect.x() >= kMaxValue ||
            screen_rect.y() < kMinValue || screen_rect.y() >= kMaxValue)
        {
            emit errorOccurred(tr("Session error: Wrong video frame position."));
            return;
        }

        delegate_->resizeDesktopFrame(screen_rect);
    }

    desktop::Frame* frame = delegate_->desktopFrame();
    if (!frame)
    {
        emit errorOccurred(tr("Session error: The desktop frame is not initialized."));
        return;
    }

    if (!video_decoder_->decode(packet, frame))
    {
        emit errorOccurred(tr("Session error: The video packet could not be decoded."));
        return;
    }

    delegate_->drawDesktopFrame();
}

void ClientDesktop::readCursorShape(const proto::desktop::CursorShape& cursor_shape)
{
    const ConnectData& connect_data = connectData();

    if (connect_data.session_type != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    uint32_t flags = connect_data.desktop_config.flags();
    if (!(flags & proto::desktop::ENABLE_CURSOR_SHAPE))
        return;

    if (!cursor_decoder_)
        cursor_decoder_ = std::make_unique<codec::CursorDecoder>();

    std::shared_ptr<desktop::MouseCursor> mouse_cursor = cursor_decoder_->decode(cursor_shape);
    if (!mouse_cursor)
        return;

    QImage image(mouse_cursor->data(),
                 mouse_cursor->size().width(),
                 mouse_cursor->size().height(),
                 mouse_cursor->stride(),
                 QImage::Format::Format_ARGB32);

    delegate_->injectCursor(
        QCursor(QPixmap::fromImage(image),
                mouse_cursor->hotSpot().x(),
                mouse_cursor->hotSpot().y()));
}

void ClientDesktop::readClipboardEvent(const proto::desktop::ClipboardEvent& clipboard_event)
{
    const ConnectData& connect_data = connectData();

    if (connect_data.session_type != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    uint32_t flags = connect_data.desktop_config.flags();
    if (!(flags & proto::desktop::ENABLE_CLIPBOARD))
        return;

    delegate_->injectClipboard(clipboard_event);
}

void ClientDesktop::readExtension(const proto::desktop::Extension& extension)
{
    if (extension.name() == common::kSelectScreenExtension)
    {
        proto::desktop::ScreenList screen_list;

        if (!screen_list.ParseFromString(extension.data()))
        {
            LOG(LS_ERROR) << "Unable to parse select screen extension data";
            return;
        }

        delegate_->setScreenList(screen_list);
    }
    else if (extension.name() == common::kSystemInfoExtension)
    {
        proto::system_info::SystemInfo system_info;

        if (!system_info.ParseFromString(extension.data()))
        {
            LOG(LS_ERROR) << "Unable to parse system info extension data";
            return;
        }

        delegate_->setSystemInfo(system_info);
    }
    else
    {
        LOG(LS_WARNING) << "Unknown extension: " << extension.name();
    }
}

} // namespace client
