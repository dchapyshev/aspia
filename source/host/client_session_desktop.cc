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

#include "host/client_session_desktop.h"

#include "base/strings/string_split.h"
#include "codec/video_encoder_vpx.h"
#include "codec/video_encoder_zstd.h"
#include "codec/video_util.h"
#include "common/desktop_session_constants.h"
#include "common/message_serialization.h"
#include "desktop/desktop_frame.h"
#include "host/desktop_session_proxy.h"
#include "host/video_frame_pump.h"
#include "net/network_channel.h"

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
        // TODO
    }
    else if (extension.name() == common::kRemoteUpdateExtension)
    {
        // TODO
    }
    else if (extension.name() == common::kSystemInfoExtension)
    {
        // TODO
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

    frame_pump_ = std::make_unique<VideoFramePump>(channelProxy(), std::move(video_encoder));
    frame_pump_->start();
}

} // namespace host
