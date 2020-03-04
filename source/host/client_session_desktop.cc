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

#include "host/client_session_desktop.h"

#include "base/power_controller.h"
#include "codec/cursor_encoder.h"
#include "codec/video_encoder_vpx.h"
#include "codec/video_encoder_zstd.h"
#include "codec/video_util.h"
#include "common/desktop_session_constants.h"
#include "common/message_serialization.h"
#include "desktop/desktop_frame.h"
#include "host/desktop_session_proxy.h"
#include "host/host_system_info.h"
#include "host/win/updater_launcher.h"
#include "net/network_channel.h"
#include "proto/desktop_internal.pb.h"

namespace host {

ClientSessionDesktop::ClientSessionDesktop(
    proto::SessionType session_type, std::unique_ptr<net::Channel> channel)
    : ClientSession(session_type, std::move(channel))
{
    // Nothing
}

ClientSessionDesktop::~ClientSessionDesktop() = default;

void ClientSessionDesktop::setDesktopSessionProxy(
    std::shared_ptr<DesktopSessionProxy> desktop_session_proxy)
{
    desktop_session_proxy_ = std::move(desktop_session_proxy);
    DCHECK(desktop_session_proxy_);
}

void ClientSessionDesktop::onMessageReceived(const base::ByteArray& buffer)
{
    incoming_message_.Clear();

    if (!common::parseMessage(buffer, &incoming_message_))
    {
        LOG(LS_ERROR) << "Invalid message from client";
        return;
    }

    if (incoming_message_.has_pointer_event())
    {
        desktop_session_proxy_->injectPointerEvent(incoming_message_.pointer_event());
    }
    else if (incoming_message_.has_key_event())
    {
        desktop_session_proxy_->injectKeyEvent(incoming_message_.key_event());
    }
    else if (incoming_message_.has_clipboard_event())
    {
        desktop_session_proxy_->injectClipboardEvent(incoming_message_.clipboard_event());
    }
    else if (incoming_message_.has_extension())
    {
        readExtension(incoming_message_.extension());
    }
    else if (incoming_message_.has_config())
    {
        readConfig(incoming_message_.config());
    }
    else
    {
        LOG(LS_WARNING) << "Unhandled message from client";
        return;
    }
}

void ClientSessionDesktop::onMessageWritten()
{
    // Nothing
}

void ClientSessionDesktop::onStarted()
{
    const char* extensions;

    // Supported extensions are different for managing and viewing the desktop.
    if (sessionType() == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        extensions = common::kSupportedExtensionsForManage;
    }
    else
    {
        DCHECK_EQ(sessionType(), proto::SESSION_TYPE_DESKTOP_VIEW);
        extensions = common::kSupportedExtensionsForView;
    }

    // Create a configuration request.
    proto::DesktopConfigRequest* request = outgoing_message_.mutable_config_request();

    // Add supported extensions and video encodings.
    request->set_extensions(extensions);
    request->set_video_encodings(common::kSupportedVideoEncodings);

    // Send the request.
    sendMessage(common::serializeMessage(outgoing_message_));
}

void ClientSessionDesktop::encodeFrame(const desktop::Frame& frame)
{
    if (!video_encoder_)
        return;

    outgoing_message_.Clear();
    proto::VideoPacket* packet = outgoing_message_.mutable_video_packet();

    // Encode the frame into a video packet.
    video_encoder_->encode(&frame, packet);

    sendMessage(common::serializeMessage(outgoing_message_));
}

void ClientSessionDesktop::encodeMouseCursor(std::shared_ptr<desktop::MouseCursor> mouse_cursor)
{
    if (!cursor_encoder_)
        return;

    outgoing_message_.Clear();

    if (cursor_encoder_->encode(std::move(mouse_cursor), outgoing_message_.mutable_cursor_shape()))
        sendMessage(common::serializeMessage(outgoing_message_));
}

void ClientSessionDesktop::setScreenList(const proto::ScreenList& list)
{
    outgoing_message_.Clear();

    proto::DesktopExtension* extension = outgoing_message_.mutable_extension();
    extension->set_name(common::kSelectScreenExtension);
    extension->set_data(list.SerializeAsString());

    sendMessage(common::serializeMessage(outgoing_message_));
}

void ClientSessionDesktop::injectClipboardEvent(const proto::ClipboardEvent& event)
{
    outgoing_message_.Clear();

    outgoing_message_.mutable_clipboard_event()->CopyFrom(event);
    sendMessage(common::serializeMessage(outgoing_message_));
}

void ClientSessionDesktop::readExtension(const proto::DesktopExtension& extension)
{
    if (extension.name() == common::kSelectScreenExtension)
    {
        proto::Screen screen;

        if (!screen.ParseFromString(extension.data()))
        {
            LOG(LS_ERROR) << "Unable to parse select screen extension data";
            return;
        }

        desktop_session_proxy_->selectScreen(screen);
    }
    else if (extension.name() == common::kPowerControlExtension)
    {
        proto::PowerControl power_control;

        if (!power_control.ParseFromString(extension.data()))
        {
            LOG(LS_ERROR) << "Unable to parse power control extension data";
            return;
        }

        switch (power_control.action())
        {
            case proto::PowerControl::ACTION_SHUTDOWN:
                base::PowerController::shutdown();
                break;

            case proto::PowerControl::ACTION_REBOOT:
                base::PowerController::reboot();
                break;

            case proto::PowerControl::ACTION_LOGOFF:
                desktop_session_proxy_->userSessionControl(
                    proto::internal::UserSessionControl::LOGOFF);
                break;

            case proto::PowerControl::ACTION_LOCK:
                desktop_session_proxy_->userSessionControl(
                    proto::internal::UserSessionControl::LOCK);
                break;

            default:
                LOG(LS_WARNING) << "Unhandled power control action: " << power_control.action();
                break;
        }
    }
    else if (extension.name() == common::kRemoteUpdateExtension)
    {
        launchUpdater(sessionId());
    }
    else if (extension.name() == common::kSystemInfoExtension)
    {
        proto::SystemInfo system_info;
        createHostSystemInfo(&system_info);

        outgoing_message_.Clear();

        proto::DesktopExtension* extension = outgoing_message_.mutable_extension();
        extension->set_name(common::kSystemInfoExtension);
        extension->set_data(system_info.SerializeAsString());

        sendMessage(common::serializeMessage(outgoing_message_));
    }
    else
    {
        LOG(LS_WARNING) << "Unknown extension: " << extension.name();
    }
}

void ClientSessionDesktop::readConfig(const proto::DesktopConfig& config)
{
    switch (config.video_encoding())
    {
        case proto::VIDEO_ENCODING_VP8:
            video_encoder_ = codec::VideoEncoderVPX::createVP8();
            break;

        case proto::VIDEO_ENCODING_VP9:
            video_encoder_ = codec::VideoEncoderVPX::createVP9();
            break;

        case proto::VIDEO_ENCODING_ZSTD:
            video_encoder_ = codec::VideoEncoderZstd::create(
                codec::parsePixelFormat(config.pixel_format()), config.compress_ratio());
            break;

        default:
        {
            // No supported video encoding.
            LOG(LS_WARNING) << "Unsupported video encoding: " << config.video_encoding();
        }
        break;
    }

    if (!video_encoder_)
    {
        LOG(LS_ERROR) << "Video encoder not initialized!";
        return;
    }

    cursor_encoder_.reset();

    if (config.flags() & proto::ENABLE_CURSOR_SHAPE)
        cursor_encoder_ = std::make_unique<codec::CursorEncoder>();

    features_.set_disable_effects(config.flags() & proto::DISABLE_DESKTOP_EFFECTS);
    features_.set_disable_wallpaper(config.flags() & proto::DISABLE_DESKTOP_WALLPAPER);
    features_.set_block_input(config.flags() & proto::BLOCK_REMOTE_INPUT);

    delegate_->onClientSessionConfigured();
}

} // namespace host
