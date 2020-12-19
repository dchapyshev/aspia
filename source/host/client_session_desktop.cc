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

#include "base/logging.h"
#include "base/power_controller.h"
#include "base/codec/audio_encoder_opus.h"
#include "base/codec/cursor_encoder.h"
#include "base/codec/scale_reducer.h"
#include "base/codec/video_encoder_vpx.h"
#include "base/desktop/frame.h"
#include "base/desktop/screen_capturer.h"
#include "common/desktop_session_constants.h"
#include "host/desktop_session_proxy.h"
#include "host/system_info.h"
#include "host/win/updater_launcher.h"
#include "proto/desktop_internal.pb.h"

namespace host {

ClientSessionDesktop::ClientSessionDesktop(
    proto::SessionType session_type, std::unique_ptr<base::NetworkChannel> channel)
    : ClientSession(session_type, std::move(channel)),
      incoming_message_(std::make_unique<proto::ClientToHost>()),
      outgoing_message_(std::make_unique<proto::HostToClient>())
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
    incoming_message_->Clear();

    if (!base::parse(buffer, incoming_message_.get()))
    {
        LOG(LS_ERROR) << "Invalid message from client";
        return;
    }

    if (incoming_message_->has_mouse_event())
    {
        if (sessionType() != proto::SESSION_TYPE_DESKTOP_MANAGE)
            return;

        if (!scale_reducer_)
            return;

        const proto::MouseEvent& mouse_event = incoming_message_->mouse_event();

        int pos_x = static_cast<int>(
            static_cast<double>(mouse_event.x() * 100) / scale_reducer_->scaleFactorX());
        int pos_y = static_cast<int>(
            static_cast<double>(mouse_event.y() * 100) / scale_reducer_->scaleFactorY());

        proto::MouseEvent out_mouse_event;
        out_mouse_event.set_mask(mouse_event.mask());
        out_mouse_event.set_x(pos_x);
        out_mouse_event.set_y(pos_y);

        desktop_session_proxy_->injectMouseEvent(out_mouse_event);
    }
    else if (incoming_message_->has_key_event())
    {
        if (sessionType() == proto::SESSION_TYPE_DESKTOP_MANAGE)
            desktop_session_proxy_->injectKeyEvent(incoming_message_->key_event());
    }
    else if (incoming_message_->has_clipboard_event())
    {
        if (sessionType() == proto::SESSION_TYPE_DESKTOP_MANAGE)
            desktop_session_proxy_->injectClipboardEvent(incoming_message_->clipboard_event());
    }
    else if (incoming_message_->has_extension())
    {
        readExtension(incoming_message_->extension());
    }
    else if (incoming_message_->has_config())
    {
        readConfig(incoming_message_->config());
    }
    else
    {
        LOG(LS_WARNING) << "Unhandled message from client";
        return;
    }
}

void ClientSessionDesktop::onMessageWritten(size_t /* pending */)
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
    proto::DesktopConfigRequest* request = outgoing_message_->mutable_config_request();

    // Add supported extensions and video encodings.
    request->set_extensions(extensions);
    request->set_video_encodings(common::kSupportedVideoEncodings);
    request->set_audio_encodings(common::kSupportedAudioEncodings);

    LOG(LS_INFO) << "Sending config request";
    LOG(LS_INFO) << "Supported extensions: " << request->extensions();
    LOG(LS_INFO) << "Supported video encodings: " << request->video_encodings();
    LOG(LS_INFO) << "Supported audio encodings: " << request->audio_encodings();

    // Send the request.
    sendMessage(base::serialize(*outgoing_message_));
}

void ClientSessionDesktop::encodeScreen(const base::Frame* frame, const base::MouseCursor* cursor)
{
    outgoing_message_->Clear();

    if (frame && video_encoder_ && scale_reducer_)
    {
        const base::Size& source_size = frame->size();
        base::Size current_size = preferred_size_;

        if (current_size.width() > source_size.width() ||
            current_size.height() > source_size.height())
        {
            current_size = source_size;
        }

        if (current_size.isEmpty())
            current_size = source_size;

        const base::Frame* scaled_frame = scale_reducer_->scaleFrame(frame, current_size);
        if (!scaled_frame)
        {
            LOG(LS_ERROR) << "No scaled frame";
            return;
        }

        proto::VideoPacket* packet = outgoing_message_->mutable_video_packet();

        // Encode the frame into a video packet.
        video_encoder_->encode(scaled_frame, packet);

        if (packet->has_format())
        {
            proto::VideoPacketFormat* format = packet->mutable_format();

            // In video packets that contain the format, we pass the screen capture type.
            format->set_capturer_type(frame->capturerType());

            // Real screen size.
            proto::Size* screen_size = format->mutable_screen_size();
            screen_size->set_width(frame->size().width());
            screen_size->set_height(frame->size().height());

            LOG(LS_INFO) << "Video packet has format";
            LOG(LS_INFO) << "Capturer type: " << base::ScreenCapturer::typeToString(
                static_cast<base::ScreenCapturer::Type>(frame->capturerType()));
            LOG(LS_INFO) << "Screen size: " << screen_size->width() << "x"
                         << screen_size->height();
            LOG(LS_INFO) << "Video size: " << format->video_rect().width() << "x"
                         << format->video_rect().height();
        }
    }

    if (cursor && cursor_encoder_)
    {
        if (!cursor_encoder_->encode(*cursor, outgoing_message_->mutable_cursor_shape()))
            outgoing_message_->clear_cursor_shape();
    }

    if (outgoing_message_->has_video_packet() || outgoing_message_->has_cursor_shape())
        sendMessage(base::serialize(*outgoing_message_));
}

void ClientSessionDesktop::encodeAudio(const proto::AudioPacket& audio_packet)
{
    if (!audio_encoder_)
        return;

    outgoing_message_->Clear();

    if (!audio_encoder_->encode(audio_packet, outgoing_message_->mutable_audio_packet()))
        return;

    sendMessage(base::serialize(*outgoing_message_));
}

void ClientSessionDesktop::setScreenList(const proto::ScreenList& list)
{
    outgoing_message_->Clear();

    proto::DesktopExtension* extension = outgoing_message_->mutable_extension();
    extension->set_name(common::kSelectScreenExtension);
    extension->set_data(list.SerializeAsString());

    sendMessage(base::serialize(*outgoing_message_));
}

void ClientSessionDesktop::injectClipboardEvent(const proto::ClipboardEvent& event)
{
    if (sessionType() == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        outgoing_message_->Clear();

        outgoing_message_->mutable_clipboard_event()->CopyFrom(event);
        sendMessage(base::serialize(*outgoing_message_));
    }
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
        preferred_size_ = base::Size();
    }
    else if (extension.name() == common::kPreferredSizeExtension)
    {
        proto::PreferredSize preferred_size;

        if (!preferred_size.ParseFromString(extension.data()))
        {
            LOG(LS_ERROR) << "Unable to parse preferred size extension data";
            return;
        }

        static const int kMaxScreenSize = std::numeric_limits<int16_t>::max();

        if (preferred_size.width() < 0 || preferred_size.width() > kMaxScreenSize ||
            preferred_size.height() < 0 || preferred_size.height() > kMaxScreenSize)
        {
            LOG(LS_ERROR) << "Invalid preferred size: "
                          << preferred_size.width() << "x" << preferred_size.height();
            return;
        }

        LOG(LS_INFO) << "Preferred size changed: "
                     << preferred_size.width() << "x" << preferred_size.height();

        preferred_size_.set(preferred_size.width(), preferred_size.height());
        desktop_session_proxy_->captureScreen();
    }
    else if (extension.name() == common::kPowerControlExtension)
    {
        if (sessionType() != proto::SESSION_TYPE_DESKTOP_MANAGE)
        {
            LOG(LS_WARNING) << "Power management is only accessible from a desktop manage session";
            return;
        }

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
                desktop_session_proxy_->control(proto::internal::Control::LOGOFF);
                break;

            case proto::PowerControl::ACTION_LOCK:
                desktop_session_proxy_->control(proto::internal::Control::LOCK);
                break;

            default:
                LOG(LS_WARNING) << "Unhandled power control action: " << power_control.action();
                break;
        }
    }
    else if (extension.name() == common::kRemoteUpdateExtension)
    {
        if (sessionType() == proto::SESSION_TYPE_DESKTOP_MANAGE)
        {
            launchUpdater(sessionId());
        }
        else
        {
            LOG(LS_WARNING) << "Update can only be launched from a desktop manage session";
        }
    }
    else if (extension.name() == common::kSystemInfoExtension)
    {
        proto::SystemInfo system_info;
        createSystemInfo(&system_info);

        outgoing_message_->Clear();

        proto::DesktopExtension* desktop_extension = outgoing_message_->mutable_extension();
        desktop_extension->set_name(common::kSystemInfoExtension);
        desktop_extension->set_data(system_info.SerializeAsString());

        sendMessage(base::serialize(*outgoing_message_));
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
            video_encoder_ = base::VideoEncoderVPX::createVP8();
            break;

        case proto::VIDEO_ENCODING_VP9:
            video_encoder_ = base::VideoEncoderVPX::createVP9();
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

    switch (config.audio_encoding())
    {
        case proto::AUDIO_ENCODING_OPUS:
            audio_encoder_ = std::make_unique<base::AudioEncoderOpus>();
            break;

        default:
        {
            LOG(LS_WARNING) << "Unsupported audio encoding: " << config.audio_encoding();
            audio_encoder_.reset();
        }
        break;
    }

    cursor_encoder_.reset();
    if (config.flags() & proto::ENABLE_CURSOR_SHAPE)
        cursor_encoder_ = std::make_unique<base::CursorEncoder>();

    scale_reducer_ = std::make_unique<base::ScaleReducer>();

    desktop_session_config_.disable_font_smoothing =
        (config.flags() & proto::DISABLE_FONT_SMOOTHING);
    desktop_session_config_.disable_effects =
        (config.flags() & proto::DISABLE_DESKTOP_EFFECTS);
    desktop_session_config_.disable_wallpaper =
        (config.flags() & proto::DISABLE_DESKTOP_WALLPAPER);
    desktop_session_config_.block_input =
        (config.flags() & proto::BLOCK_REMOTE_INPUT);
    desktop_session_config_.lock_at_disconnect =
        (config.flags() & proto::LOCK_AT_DISCONNECT);

    LOG(LS_INFO) << "Client configuration changed";
    LOG(LS_INFO) << "Video encoding: " << config.video_encoding();
    LOG(LS_INFO) << "Enable cursor shape: " << (cursor_encoder_ != nullptr);
    LOG(LS_INFO) << "Disable font smoothing: " << desktop_session_config_.disable_font_smoothing;
    LOG(LS_INFO) << "Disable desktop effects: " << desktop_session_config_.disable_effects;
    LOG(LS_INFO) << "Disable desktop wallpaper: " << desktop_session_config_.disable_wallpaper;
    LOG(LS_INFO) << "Block input: " << desktop_session_config_.block_input;
    LOG(LS_INFO) << "Lock at disconnect: " << desktop_session_config_.lock_at_disconnect;

    delegate_->onClientSessionConfigured();
}

} // namespace host
