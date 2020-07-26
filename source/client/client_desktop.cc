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
#include "base/codec/cursor_decoder.h"
#include "base/codec/video_decoder.h"
#include "base/codec/video_util.h"
#include "base/desktop/frame.h"
#include "base/desktop/mouse_cursor.h"
#include "client/desktop_control_proxy.h"
#include "client/desktop_window.h"
#include "client/desktop_window_proxy.h"
#include "client/config_factory.h"
#include "common/desktop_session_constants.h"

namespace client {

namespace {

int calculateFps(int last_fps, std::chrono::milliseconds duration, int64_t count)
{
    static const double kAlpha = 0.1;
    return int((kAlpha * ((1000.0 / double(duration.count())) * double(count))) +
        ((1.0 - kAlpha) * double(last_fps)));
}

size_t calculateAvgVideoSize(size_t last_avg_size, size_t bytes)
{
    static const double kAlpha = 0.1;
    return size_t((kAlpha * double(bytes)) + ((1.0 - kAlpha) * double(last_avg_size)));
}

} // namespace

ClientDesktop::ClientDesktop(std::shared_ptr<base::TaskRunner> io_task_runner)
    : Client(io_task_runner),
      desktop_control_proxy_(std::make_shared<DesktopControlProxy>(io_task_runner, this))
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
    start_time_ = Clock::now();
    started_ = true;

    input_event_filter_.setSessionType(sessionType());
    desktop_window_proxy_->showWindow(desktop_control_proxy_, peer_version);
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

void ClientDesktop::onMessageWritten(size_t /* pending */)
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

    input_event_filter_.setClipboardEnabled(desktop_config_.flags() & proto::ENABLE_CLIPBOARD);

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

void ClientDesktop::setPreferredSize(int width, int height)
{
    outgoing_message_.Clear();

    proto::PreferredSize preferred_size;
    preferred_size.set_width(width);
    preferred_size.set_height(height);

    proto::DesktopExtension* extension = outgoing_message_.mutable_extension();

    extension->set_name(common::kPreferredSizeExtension);
    extension->set_data(preferred_size.SerializeAsString());

    sendMessage(outgoing_message_);
}

void ClientDesktop::onKeyEvent(const proto::KeyEvent& event)
{
    std::optional<proto::KeyEvent> out_event = input_event_filter_.keyEvent(event);
    if (!out_event.has_value())
        return;

    outgoing_message_.Clear();
    outgoing_message_.mutable_key_event()->CopyFrom(out_event.value());

    sendMessage(outgoing_message_);
}

void ClientDesktop::onMouseEvent(const proto::MouseEvent& event)
{
    std::optional<proto::MouseEvent> out_event = input_event_filter_.mouseEvent(event);
    if (!out_event.has_value())
        return;

    outgoing_message_.Clear();
    outgoing_message_.mutable_mouse_event()->CopyFrom(out_event.value());

    sendMessage(outgoing_message_);
}

void ClientDesktop::onClipboardEvent(const proto::ClipboardEvent& event)
{
    std::optional<proto::ClipboardEvent> out_event = input_event_filter_.sendClipboardEvent(event);
    if (!out_event.has_value())
        return;

    outgoing_message_.Clear();
    outgoing_message_.mutable_clipboard_event()->CopyFrom(out_event.value());
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

void ClientDesktop::onMetricsRequest()
{
    TimePoint current_time = Clock::now();

    std::chrono::milliseconds fps_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(current_time - begin_time_);

    fps_ = calculateFps(fps_, fps_duration, video_frame_count_);

    begin_time_ = current_time;
    video_frame_count_ = 0;

    std::chrono::seconds session_duration =
        std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time_);

    DesktopWindow::Metrics metrics;
    metrics.duration = session_duration;
    metrics.total_rx = totalRx();
    metrics.total_tx = totalTx();
    metrics.speed_rx = speedRx();
    metrics.speed_tx = speedTx();
    metrics.min_video_packet = min_video_packet_;
    metrics.max_video_packet = max_video_packet_;
    metrics.avg_video_packet = avg_video_packet_;
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
        video_decoder_ = base::VideoDecoder::create(packet.encoding());
        video_encoding_ = packet.encoding();
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
        static const int kMinValue = -std::numeric_limits<uint16_t>::max();

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

    ++video_frame_count_;

    size_t packet_size = packet.ByteSizeLong();

    avg_video_packet_ = calculateAvgVideoSize(avg_video_packet_, packet_size);
    min_video_packet_ = std::min(min_video_packet_, packet_size);
    max_video_packet_ = std::max(max_video_packet_, packet_size);

    desktop_window_proxy_->drawFrame();
}

void ClientDesktop::readCursorShape(const proto::CursorShape& cursor_shape)
{
    if (sessionType() != proto::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    if (!(desktop_config_.flags() & proto::ENABLE_CURSOR_SHAPE))
        return;

    if (!cursor_decoder_)
        cursor_decoder_ = std::make_unique<base::CursorDecoder>();

    std::shared_ptr<base::MouseCursor> mouse_cursor = cursor_decoder_->decode(cursor_shape);
    if (!mouse_cursor)
        return;

    desktop_window_proxy_->setMouseCursor(mouse_cursor);
}

void ClientDesktop::readClipboardEvent(const proto::ClipboardEvent& event)
{
    std::optional<proto::ClipboardEvent> out_event = input_event_filter_.readClipboardEvent(event);
    if (!out_event.has_value())
        return;

    desktop_window_proxy_->injectClipboardEvent(out_event.value());
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
