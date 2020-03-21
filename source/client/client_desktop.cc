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

#include "client/client_desktop.h"

#include "base/logging.h"
#include "client/desktop_control_proxy.h"
#include "client/desktop_window.h"
#include "client/desktop_window_proxy.h"
#include "client/config_factory.h"
#include "codec/cursor_decoder.h"
#include "codec/video_decoder.h"
#include "codec/video_util.h"
#include "common/desktop_session_constants.h"
#include "desktop/desktop_frame.h"
#include "desktop/mouse_cursor.h"

namespace client {

ClientDesktop::ClientDesktop(std::shared_ptr<base::TaskRunner> ui_task_runner)
    : Client(std::move(ui_task_runner))
{
    // Nothing
}

ClientDesktop::~ClientDesktop() = default;

void ClientDesktop::setDesktopWindow(DesktopWindow* desktop_window)
{
    DCHECK(uiTaskRunner()->belongsToCurrentThread());

    desktop_window_proxy_ = DesktopWindowProxy::create(uiTaskRunner(), desktop_window);
}

void ClientDesktop::onSessionStarted(const base::Version& peer_version)
{
    desktop_control_proxy_ = std::make_shared<DesktopControlProxy>(ioTaskRunner(), this);
    started_ = true;

    desktop_window_proxy_->showWindow(desktop_control_proxy_, peer_version);
}

void ClientDesktop::onSessionStopped()
{
    started_ = false;
    desktop_control_proxy_->dettach();
}

void ClientDesktop::onMessageReceived(const base::ByteArray& buffer)
{
    incoming_message_.Clear();

    if (!incoming_message_.ParseFromArray(buffer.data(), buffer.size()))
    {
        LOG(LS_ERROR) << "Invalid message from host";
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

void ClientDesktop::onMessageWritten()
{
    // Nothing
}

void ClientDesktop::setDesktopConfig(const proto::DesktopConfig& desktop_config)
{
    desktop_config_ = desktop_config;

    ConfigFactory::fixupDesktopConfig(&desktop_config_);

    // If the session is not already running, then we do not need to send the configuration.
    if (!started_)
        return;

    if (!(desktop_config_.flags() & proto::ENABLE_CURSOR_SHAPE))
        cursor_decoder_.reset();

    outgoing_message_.Clear();
    outgoing_message_.mutable_config()->CopyFrom(desktop_config_);
    sendMessage(outgoing_message_);
}

void ClientDesktop::setCurrentScreen(const proto::Screen& screen)
{
    outgoing_message_.Clear();

    proto::DesktopExtension* extension = outgoing_message_.mutable_extension();

    extension->set_name(common::kSelectScreenExtension);
    extension->set_data(screen.SerializeAsString());

    sendMessage(outgoing_message_);
}

void ClientDesktop::onKeyEvent(const proto::KeyEvent& event)
{
    if (sessionType() != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    outgoing_message_.Clear();
    outgoing_message_.mutable_key_event()->CopyFrom(event);
    sendMessage(outgoing_message_);
}

void ClientDesktop::onPointerEvent(const proto::PointerEvent& event)
{
    if (sessionType() != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    outgoing_message_.Clear();
    outgoing_message_.mutable_pointer_event()->CopyFrom(event);
    sendMessage(outgoing_message_);
}

void ClientDesktop::onClipboardEvent(const proto::ClipboardEvent& event)
{
    if (sessionType() != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    if (!(desktop_config_.flags() & proto::ENABLE_CLIPBOARD))
        return;

    outgoing_message_.Clear();
    outgoing_message_.mutable_clipboard_event()->CopyFrom(event);
    sendMessage(outgoing_message_);
}

void ClientDesktop::onPowerControl(proto::PowerControl::Action action)
{
    if (sessionType() != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    outgoing_message_.Clear();

    proto::DesktopExtension* extension = outgoing_message_.mutable_extension();

    proto::PowerControl power_control;
    power_control.set_action(action);

    extension->set_name(common::kPowerControlExtension);
    extension->set_data(power_control.SerializeAsString());

    sendMessage(outgoing_message_);
}

void ClientDesktop::onRemoteUpdate()
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_extension()->set_name(common::kRemoteUpdateExtension);
    sendMessage(outgoing_message_);
}

void ClientDesktop::onSystemInfoRequest()
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_extension()->set_name(common::kSystemInfoExtension);
    sendMessage(outgoing_message_);
}

void ClientDesktop::readConfigRequest(const proto::DesktopConfigRequest& config_request)
{
    // We notify the window about changes in the list of extensions and video encodings.
    // A window can disable/enable some of its capabilities in accordance with this information.
    desktop_window_proxy_->setCapabilities(
        config_request.extensions(), config_request.video_encodings());

    // If current video encoding not supported.
    if (!(config_request.video_encodings() & desktop_config_.video_encoding()))
    {
        // We tell the window about the need to change the encoding.
        desktop_window_proxy_->configRequired();
    }
    else
    {
        // Everything is fine, we send the current configuration.
        setDesktopConfig(desktop_config_);
    }
}

void ClientDesktop::readVideoPacket(const proto::VideoPacket& packet)
{
    if (video_encoding_ != packet.encoding())
    {
        video_decoder_ = codec::VideoDecoder::create(packet.encoding());
        video_encoding_ = packet.encoding();
    }

    if (!video_decoder_)
    {
        LOG(LS_ERROR) << "Video decoder not initialized";
        return;
    }

    if (packet.has_format())
    {
        const proto::Rect& screen_rect = packet.format().screen_rect();

        static const int kMaxValue = std::numeric_limits<uint16_t>::max();
        static const int kMinValue = -std::numeric_limits<uint16_t>::max();

        if (screen_rect.width()  <= 0 || screen_rect.width()  >= kMaxValue ||
            screen_rect.height() <= 0 || screen_rect.height() >= kMaxValue)
        {
            LOG(LS_ERROR) << "Wrong video frame size";
            return;
        }

        if (screen_rect.x() < kMinValue || screen_rect.x() >= kMaxValue ||
            screen_rect.y() < kMinValue || screen_rect.y() >= kMaxValue)
        {
            LOG(LS_ERROR) << "Wrong video frame position";
            return;
        }

        desktop_frame_ = desktop_window_proxy_->allocateFrame(
            desktop::Size(screen_rect.width(), screen_rect.height()));

        desktop_frame_->setTopLeft(desktop::Point(screen_rect.x(), screen_rect.y()));
    }

    if (!desktop_frame_)
    {
        LOG(LS_ERROR) << "The desktop frame is not initialized";
        return;
    }

    if (!video_decoder_->decode(packet, desktop_frame_.get()))
    {
        LOG(LS_ERROR) << "The video packet could not be decoded";
        return;
    }

    desktop_window_proxy_->drawFrame(desktop_frame_);
}

void ClientDesktop::readCursorShape(const proto::CursorShape& cursor_shape)
{
    if (sessionType() != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    if (!(desktop_config_.flags() & proto::ENABLE_CURSOR_SHAPE))
        return;

    if (!cursor_decoder_)
        cursor_decoder_ = std::make_unique<codec::CursorDecoder>();

    std::shared_ptr<desktop::MouseCursor> mouse_cursor = cursor_decoder_->decode(cursor_shape);
    if (!mouse_cursor)
        return;

    desktop_window_proxy_->drawMouseCursor(mouse_cursor);
}

void ClientDesktop::readClipboardEvent(const proto::ClipboardEvent& clipboard_event)
{
    if (sessionType() != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    if (!(desktop_config_.flags() & proto::ENABLE_CLIPBOARD))
        return;

    desktop_window_proxy_->injectClipboardEvent(clipboard_event);
}

void ClientDesktop::readExtension(const proto::DesktopExtension& extension)
{
    if (extension.name() == common::kSelectScreenExtension)
    {
        proto::ScreenList screen_list;

        if (!screen_list.ParseFromString(extension.data()))
        {
            LOG(LS_ERROR) << "Unable to parse select screen extension data";
            return;
        }

        desktop_window_proxy_->setScreenList(screen_list);
    }
    else if (extension.name() == common::kSystemInfoExtension)
    {
        proto::SystemInfo system_info;

        if (!system_info.ParseFromString(extension.data()))
        {
            LOG(LS_ERROR) << "Unable to parse system info extension data";
            return;
        }

        desktop_window_proxy_->setSystemInfo(system_info);
    }
    else
    {
        LOG(LS_WARNING) << "Unknown extension: " << extension.name();
    }
}

} // namespace client
