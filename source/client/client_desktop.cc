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
#include "base/task_runner.h"
#include "base/audio/audio_player.h"
#include "base/codec/audio_decoder_opus.h"
#include "base/codec/cursor_decoder.h"
#include "base/codec/video_decoder.h"
#include "base/desktop/mouse_cursor.h"
#include "client/desktop_control_proxy.h"
#include "client/desktop_window.h"
#include "client/desktop_window_proxy.h"
#include "client/config_factory.h"
#include "common/desktop_session_constants.h"

namespace client {

namespace {

int calculateFps(int last_fps, const std::chrono::milliseconds& duration, int64_t count)
{
    static const double kAlpha = 0.1;
    return static_cast<int>(
        (kAlpha * ((1000.0 / static_cast<double>(duration.count())) * static_cast<double>(count))) +
        ((1.0 - kAlpha) * static_cast<double>(last_fps)));
}

size_t calculateAvgSize(size_t last_avg_size, size_t bytes)
{
    static const double kAlpha = 0.1;
    return static_cast<size_t>(
        (kAlpha * static_cast<double>(bytes)) +
        ((1.0 - kAlpha) * static_cast<double>(last_avg_size)));
}

} // namespace

ClientDesktop::ClientDesktop(std::shared_ptr<base::TaskRunner> io_task_runner)
    : Client(io_task_runner),
      desktop_control_proxy_(std::make_shared<DesktopControlProxy>(io_task_runner, this)),
      incoming_message_(std::make_unique<proto::HostToClient>()),
      outgoing_message_(std::make_unique<proto::ClientToHost>()),
      audio_player_(base::AudioPlayer::create())
{
    // Nothing
}

ClientDesktop::~ClientDesktop()
{
    desktop_control_proxy_->dettach();
}

void ClientDesktop::setDesktopWindow(std::shared_ptr<DesktopWindowProxy> desktop_window_proxy)
{
    desktop_window_proxy_ = std::move(desktop_window_proxy);
}

void ClientDesktop::onSessionStarted(const base::Version& peer_version)
{
    LOG(LS_INFO) << "Desktop session started";

    start_time_ = Clock::now();
    started_ = true;

    input_event_filter_.setSessionType(sessionType());
    desktop_window_proxy_->showWindow(desktop_control_proxy_, peer_version);

    clipboard_monitor_ = std::make_unique<common::ClipboardMonitor>();
    clipboard_monitor_->start(ioTaskRunner(), this);
}

void ClientDesktop::onMessageReceived(const base::ByteArray& buffer)
{
    incoming_message_->Clear();

    if (!base::parse(buffer, incoming_message_.get()))
    {
        LOG(LS_ERROR) << "Invalid message from host";
        return;
    }

    if (incoming_message_->has_video_packet() || incoming_message_->has_cursor_shape())
    {
        if (incoming_message_->has_video_packet())
            readVideoPacket(incoming_message_->video_packet());

        if (incoming_message_->has_cursor_shape())
            readCursorShape(incoming_message_->cursor_shape());
    }
    else if (incoming_message_->has_audio_packet())
    {
        readAudioPacket(incoming_message_->audio_packet());
    }
    else if (incoming_message_->has_clipboard_event())
    {
        readClipboardEvent(incoming_message_->clipboard_event());
    }
    else if (incoming_message_->has_config_request())
    {
        readConfigRequest(incoming_message_->config_request());
    }
    else if (incoming_message_->has_extension())
    {
        readExtension(incoming_message_->extension());
    }
    else
    {
        // Unknown messages are ignored.
        LOG(LS_WARNING) << "Unhandled message from host";
    }
}

void ClientDesktop::onMessageWritten(size_t /* pending */)
{
    // Nothing
}

void ClientDesktop::onClipboardEvent(const proto::ClipboardEvent& event)
{
    std::optional<proto::ClipboardEvent> out_event = input_event_filter_.sendClipboardEvent(event);
    if (!out_event.has_value())
        return;

    outgoing_message_->Clear();
    outgoing_message_->mutable_clipboard_event()->CopyFrom(out_event.value());
    sendMessage(*outgoing_message_);
}

void ClientDesktop::setDesktopConfig(const proto::DesktopConfig& desktop_config)
{
    LOG(LS_INFO) << "setDesktopConfig called";
    desktop_config_ = desktop_config;

    ConfigFactory::fixupDesktopConfig(&desktop_config_);

    // If the session is not already running, then we do not need to send the configuration.
    if (!started_)
    {
        LOG(LS_INFO) << "Session not started yet";
        return;
    }

    if (!(desktop_config_.flags() & proto::ENABLE_CURSOR_SHAPE))
    {
        LOG(LS_INFO) << "Cursor shape disabled";
        cursor_decoder_.reset();
    }

    input_event_filter_.setClipboardEnabled(desktop_config_.flags() & proto::ENABLE_CLIPBOARD);

    outgoing_message_->Clear();
    outgoing_message_->mutable_config()->CopyFrom(desktop_config_);

    LOG(LS_INFO) << "Send new config to host";
    sendMessage(*outgoing_message_);
}

void ClientDesktop::setCurrentScreen(const proto::Screen& screen)
{
    LOG(LS_INFO) << "Current screen changed: " << screen.id();

    outgoing_message_->Clear();
    proto::DesktopExtension* extension = outgoing_message_->mutable_extension();

    extension->set_name(common::kSelectScreenExtension);
    extension->set_data(screen.SerializeAsString());

    sendMessage(*outgoing_message_);
}

void ClientDesktop::setPreferredSize(int width, int height)
{
    LOG(LS_INFO) << "Preferred size changed: " << width << "x" << height;

    outgoing_message_->Clear();

    proto::PreferredSize preferred_size;
    preferred_size.set_width(width);
    preferred_size.set_height(height);

    proto::DesktopExtension* extension = outgoing_message_->mutable_extension();

    extension->set_name(common::kPreferredSizeExtension);
    extension->set_data(preferred_size.SerializeAsString());

    sendMessage(*outgoing_message_);
}

void ClientDesktop::onKeyEvent(const proto::KeyEvent& event)
{
    std::optional<proto::KeyEvent> out_event = input_event_filter_.keyEvent(event);
    if (!out_event.has_value())
        return;

    outgoing_message_->Clear();
    outgoing_message_->mutable_key_event()->CopyFrom(out_event.value());

    sendMessage(*outgoing_message_);
}

void ClientDesktop::onMouseEvent(const proto::MouseEvent& event)
{
    std::optional<proto::MouseEvent> out_event = input_event_filter_.mouseEvent(event);
    if (!out_event.has_value())
        return;

    outgoing_message_->Clear();
    outgoing_message_->mutable_mouse_event()->CopyFrom(out_event.value());

    sendMessage(*outgoing_message_);
}

void ClientDesktop::onPowerControl(proto::PowerControl::Action action)
{
    if (sessionType() != proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(LS_INFO) << "Power control supported only for desktop manage session";
        return;
    }

    outgoing_message_->Clear();

    proto::DesktopExtension* extension = outgoing_message_->mutable_extension();

    proto::PowerControl power_control;
    power_control.set_action(action);

    extension->set_name(common::kPowerControlExtension);
    extension->set_data(power_control.SerializeAsString());

    sendMessage(*outgoing_message_);
}

void ClientDesktop::onRemoteUpdate()
{
    outgoing_message_->Clear();
    outgoing_message_->mutable_extension()->set_name(common::kRemoteUpdateExtension);
    sendMessage(*outgoing_message_);
}

void ClientDesktop::onSystemInfoRequest()
{
    outgoing_message_->Clear();
    outgoing_message_->mutable_extension()->set_name(common::kSystemInfoExtension);
    sendMessage(*outgoing_message_);
}

void ClientDesktop::onMetricsRequest()
{
    TimePoint current_time = Clock::now();

    if (fps_time_ != TimePoint())
    {
        std::chrono::milliseconds fps_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(current_time - fps_time_);
        fps_ = calculateFps(fps_, fps_duration, fps_frame_count_);
    }
    else
    {
        fps_ = 0;
    }

    fps_time_ = current_time;
    fps_frame_count_ = 0;

    std::chrono::seconds session_duration =
        std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time_);

    DesktopWindow::Metrics metrics;
    metrics.duration = session_duration;
    metrics.total_rx = totalRx();
    metrics.total_tx = totalTx();
    metrics.speed_rx = speedRx();
    metrics.speed_tx = speedTx();

    if (min_video_packet_ != std::numeric_limits<size_t>::max())
        metrics.min_video_packet = min_video_packet_;
    else
        metrics.min_video_packet = 0;

    metrics.max_video_packet = max_video_packet_;
    metrics.avg_video_packet = avg_video_packet_;
    metrics.video_packet_count = video_packet_count_;

    if (min_audio_packet_ != std::numeric_limits<size_t>::max())
        metrics.min_audio_packet = min_audio_packet_;
    else
        metrics.min_audio_packet = 0;

    metrics.max_audio_packet = max_audio_packet_;
    metrics.avg_audio_packet = avg_audio_packet_;
    metrics.audio_packet_count = audio_packet_count_;

    metrics.video_capturer_type = video_capturer_type_;
    metrics.fps = fps_;
    metrics.send_mouse = input_event_filter_.sendMouseCount();
    metrics.drop_mouse = input_event_filter_.dropMouseCount();
    metrics.send_key   = input_event_filter_.sendKeyCount();
    metrics.read_clipboard = input_event_filter_.readClipboardCount();
    metrics.send_clipboard = input_event_filter_.sendClipboardCount();

    desktop_window_proxy_->setMetrics(metrics);
}

void ClientDesktop::readConfigRequest(const proto::DesktopConfigRequest& config_request)
{
    LOG(LS_INFO) << "Config request received";

    // We notify the window about changes in the list of extensions and video encodings.
    // A window can disable/enable some of its capabilities in accordance with this information.
    desktop_window_proxy_->setCapabilities(
        config_request.extensions(), config_request.video_encodings());

    // If current video encoding not supported.
    if (!(config_request.video_encodings() & desktop_config_.video_encoding()))
    {
        LOG(LS_WARNING) << "Current video encoding not supported";

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
        video_decoder_ = base::VideoDecoder::create(packet.encoding());
        video_encoding_ = packet.encoding();

        LOG(LS_INFO) << "Video encoding changed to: " << video_encoding_;
    }

    if (!video_decoder_)
    {
        LOG(LS_ERROR) << "Video decoder not initialized";
        return;
    }

    if (packet.has_format())
    {
        const proto::VideoPacketFormat& format = packet.format();
        base::Size video_size(format.video_rect().width(), format.video_rect().height());
        base::Size screen_size = video_size;

        static const int kMaxValue = std::numeric_limits<uint16_t>::max();

        if (video_size.width()  <= 0 || video_size.width()  >= kMaxValue ||
            video_size.height() <= 0 || video_size.height() >= kMaxValue)
        {
            LOG(LS_ERROR) << "Wrong video frame size";
            return;
        }

        if (format.has_screen_size())
        {
            screen_size = base::Size(
                format.screen_size().width(), format.screen_size().height());

            if (screen_size.width() <= 0 || screen_size.width() >= kMaxValue ||
                screen_size.height() <= 0 || screen_size.height() >= kMaxValue)
            {
                LOG(LS_ERROR) << "Wrong screen size";
                return;
            }
        }

        video_capturer_type_ = format.capturer_type();

        LOG(LS_INFO) << "New video size: " << video_size.width() << "x" << video_size.height();
        LOG(LS_INFO) << "New screen size: " << screen_size.width() << "x" << screen_size.height();

        desktop_frame_ = desktop_window_proxy_->allocateFrame(video_size);
        desktop_window_proxy_->setFrame(screen_size, desktop_frame_);
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

    ++video_packet_count_;
    ++fps_frame_count_;

    size_t packet_size = packet.ByteSizeLong();

    avg_video_packet_ = calculateAvgSize(avg_video_packet_, packet_size);
    min_video_packet_ = std::min(min_video_packet_, packet_size);
    max_video_packet_ = std::max(max_video_packet_, packet_size);

    desktop_window_proxy_->drawFrame();
}

void ClientDesktop::readAudioPacket(const proto::AudioPacket& packet)
{
    if (!audio_player_)
        return;

    if (packet.encoding() != audio_encoding_)
    {
        audio_decoder_ = base::AudioDecoder::create(packet.encoding());
        audio_encoding_ = packet.encoding();

        LOG(LS_INFO) << "Audio encoding changed to: " << audio_encoding_;
    }

    if (!audio_decoder_)
        return;

    size_t packet_size = packet.ByteSizeLong();

    avg_audio_packet_ = calculateAvgSize(avg_audio_packet_, packet_size);
    min_audio_packet_ = std::min(min_audio_packet_, packet_size);
    max_audio_packet_ = std::max(max_audio_packet_, packet_size);

    ++audio_packet_count_;

    std::unique_ptr<proto::AudioPacket> decoded_packet = audio_decoder_->decode(packet);
    if (decoded_packet)
        audio_player_->addPacket(std::move(decoded_packet));
}

void ClientDesktop::readCursorShape(const proto::CursorShape& cursor_shape)
{
    if (sessionType() != proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(LS_WARNING) << "Cursor shape received not session type not desktop manage";
        return;
    }

    if (!(desktop_config_.flags() & proto::ENABLE_CURSOR_SHAPE))
    {
        LOG(LS_WARNING) << "Cursor shape received not disabled in client";
        return;
    }

    if (!cursor_decoder_)
    {
        LOG(LS_INFO) << "Cursor decoder initialization";
        cursor_decoder_ = std::make_unique<base::CursorDecoder>();
    }

    std::shared_ptr<base::MouseCursor> mouse_cursor = cursor_decoder_->decode(cursor_shape);
    if (!mouse_cursor)
        return;

    desktop_window_proxy_->setMouseCursor(mouse_cursor);
}

void ClientDesktop::readClipboardEvent(const proto::ClipboardEvent& event)
{
    if (!clipboard_monitor_)
    {
        LOG(LS_WARNING) << "Clipboard received not disabled in client";
        return;
    }

    std::optional<proto::ClipboardEvent> out_event = input_event_filter_.readClipboardEvent(event);
    if (!out_event.has_value())
        return;

    clipboard_monitor_->injectClipboardEvent(out_event.value());
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

        LOG(LS_INFO) << "Screen list received";
        LOG(LS_INFO) << "Primary screen: " << screen_list.primary_screen();
        LOG(LS_INFO) << "Current screen: " << screen_list.current_screen();

        for (int i = 0; i < screen_list.screen_size(); ++i)
        {
            const proto::Screen& screen = screen_list.screen(i);
            LOG(LS_INFO) << "Screen #" << i << ": id=" << screen.id() << ", title=" << screen.title();
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
