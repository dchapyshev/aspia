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
#include "base/strings/string_split.h"
#include "codec/cursor_encoder.h"
#include "codec/video_encoder_vpx.h"
#include "codec/video_encoder_zstd.h"
#include "codec/video_util.h"
#include "common/desktop_session_constants.h"
#include "common/message_serialization.h"
#include "desktop/desktop_frame.h"
#include "host/desktop_session_proxy.h"
#include "host/host_system_info.h"
#include "host/video_frame_pump.h"
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
    proto::ClientToHost message;

    if (!common::parseMessage(buffer, &message))
    {
        LOG(LS_ERROR) << "Invalid message from client";
        return;
    }

    if (message.has_pointer_event())
    {
        desktop_session_proxy_->injectPointerEvent(message.pointer_event());
    }
    else if (message.has_key_event())
    {
        desktop_session_proxy_->injectKeyEvent(message.key_event());
    }
    else if (message.has_clipboard_event())
    {
        desktop_session_proxy_->injectClipboardEvent(message.clipboard_event());
    }
    else if (message.has_extension())
    {
        readExtension(message.extension());
    }
    else if (message.has_config())
    {
        readConfig(message.config());
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

    // Add supported extensions to the list.
    extensions_ = base::splitString(extensions, ";", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

    proto::HostToClient message;

    // Create a configuration request.
    proto::DesktopConfigRequest* request = message.mutable_config_request();

    // Add supported extensions and video encodings.
    request->set_extensions(extensions);
    request->set_video_encodings(common::kSupportedVideoEncodings);

    // Send the request.
    sendMessage(common::serializeMessage(message));
}

void ClientSessionDesktop::encodeFrame(const desktop::Frame& frame)
{
    if (frame_pump_)
        frame_pump_->encodeFrame(frame);
}

void ClientSessionDesktop::encodeMouseCursor(std::shared_ptr<desktop::MouseCursor> mouse_cursor)
{
    if (!cursor_encoder_)
        return;

    proto::HostToClient message;
    if (cursor_encoder_->encode(std::move(mouse_cursor), message.mutable_cursor_shape()))
        sendMessage(common::serializeMessage(message));
}

void ClientSessionDesktop::setScreenList(const proto::ScreenList& list)
{
    proto::HostToClient message;

    proto::DesktopExtension* extension = message.mutable_extension();
    extension->set_name(common::kSelectScreenExtension);
    extension->set_data(list.SerializeAsString());

    sendMessage(common::serializeMessage(message));
}

void ClientSessionDesktop::injectClipboardEvent(const proto::ClipboardEvent& event)
{
    proto::HostToClient message;
    message.mutable_clipboard_event()->CopyFrom(event);
    sendMessage(common::serializeMessage(message));
}

void ClientSessionDesktop::readExtension(const proto::DesktopExtension& extension)
{
    bool extension_found = false;

    for (const auto& item : extensions_)
    {
        if (item == extension.name())
        {
            extension_found = true;
            break;
        }
    }

    if (!extension_found)
    {
        DLOG(LS_WARNING) << "Unsupported or disabled extensions: " << extension.name();
        return;
    }

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
        // FIXME: Running in session the current user, not console.
        launchUpdater(WTSGetActiveConsoleSessionId());
    }
    else if (extension.name() == common::kSystemInfoExtension)
    {
        proto::SystemInfo system_info;
        createHostSystemInfo(&system_info);

        proto::HostToClient message;

        proto::DesktopExtension* extension = message.mutable_extension();
        extension->set_name(common::kSystemInfoExtension);
        extension->set_data(system_info.SerializeAsString());

        sendMessage(common::serializeMessage(message));
    }
    else
    {
        LOG(LS_WARNING) << "Unknown extension: " << extension.name();
    }
}

void ClientSessionDesktop::readConfig(const proto::DesktopConfig& config)
{
    std::unique_ptr<codec::VideoEncoder> video_encoder;

    switch (config.video_encoding())
    {
        case proto::VIDEO_ENCODING_VP8:
            video_encoder = codec::VideoEncoderVPX::createVP8();
            break;

        case proto::VIDEO_ENCODING_VP9:
            video_encoder = codec::VideoEncoderVPX::createVP9();
            break;

        case proto::VIDEO_ENCODING_ZSTD:
            video_encoder = codec::VideoEncoderZstd::create(
                codec::parsePixelFormat(config.pixel_format()), config.compress_ratio());
            break;

        default:
        {
            // No supported video encoding.
            LOG(LS_WARNING) << "Unsupported video encoding: " << config.video_encoding();
        }
        break;
    }

    if (!video_encoder)
    {
        LOG(LS_ERROR) << "Video encoder not initialized!";
        return;
    }

    cursor_encoder_.reset();

    if (config.flags() & proto::ENABLE_CURSOR_SHAPE)
        cursor_encoder_ = std::make_unique<codec::CursorEncoder>();

    proto::internal::EnableFeatures features;
    features.set_effects(!(config.flags() & proto::DISABLE_DESKTOP_EFFECTS));
    features.set_wallpaper(!(config.flags() & proto::DISABLE_DESKTOP_WALLPAPER));
    features.set_block_input(config.flags() & proto::BLOCK_REMOTE_INPUT);

    desktop_session_proxy_->enableFeatures(features);

    frame_pump_ = std::make_unique<VideoFramePump>(channelProxy(), std::move(video_encoder));
    frame_pump_->start();
}

} // namespace host
