//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QTimer>

#include "base/logging.h"
#include "base/codec/audio_decoder.h"
#include "base/audio/audio_player.h"
#include "base/codec/cursor_decoder.h"
#include "base/codec/video_decoder.h"
#include "base/codec/webm_file_writer.h"
#include "base/codec/webm_video_encoder.h"
#include "base/desktop/frame_aligned.h"
#include "base/desktop/mouse_cursor.h"
#include "common/desktop_session_constants.h"
#include "proto/desktop_channel.h"
#include "proto/desktop_file.h"
#include "proto/desktop_user.h"

namespace client {

namespace {

//--------------------------------------------------------------------------------------------------
int calculateFps(int last_fps, const std::chrono::milliseconds& duration, qint64 count)
{
    static const double kAlpha = 0.1;
    return static_cast<int>(
        (kAlpha * ((1000.0 / static_cast<double>(duration.count())) * static_cast<double>(count))) +
        ((1.0 - kAlpha) * static_cast<double>(last_fps)));
}

//--------------------------------------------------------------------------------------------------
size_t calculateAvgSize(size_t last_avg_size, size_t bytes)
{
    static const double kAlpha = 0.1;
    return static_cast<size_t>(
        (kAlpha * static_cast<double>(bytes)) +
        ((1.0 - kAlpha) * static_cast<double>(last_avg_size)));
}

} // namespace

//--------------------------------------------------------------------------------------------------
ClientDesktop::ClientDesktop(QObject* parent)
    : Client(parent)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientDesktop::~ClientDesktop()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onStarted()
{
    CLOG(INFO) << "Desktop session started";

    start_time_ = Clock::now();
    key_frame_received_ = false;
    started_ = true;

    input_event_filter_.setSessionType(sessionState()->sessionType());

    if (sessionState()->sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        clipboard_monitor_ = new common::ClipboardMonitor(this);
        connect(clipboard_monitor_, &common::ClipboardMonitor::sig_clipboardEvent,
                this, &ClientDesktop::onClipboardEvent);
        clipboard_monitor_->start();
    }

    audio_player_ = base::AudioPlayer::create();
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::desktop::CHANNEL_ID_VIDEO)
    {
        proto::video::HostToClient message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse video message";
            return;
        }

        if (message.has_packet())
            readVideoPacket(message.packet());
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_CURSOR)
    {
        proto::cursor::HostToClient message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse cursor message";
            return;
        }

        if (message.has_shape())
            readCursorShape(message.shape());
        else if (message.has_position())
            readCursorPosition(message.position());
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_SCREEN)
    {
        proto::screen::HostToClient message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse screen message";
            return;
        }

        if (message.has_screen_list())
            emit sig_screenListChanged(message.screen_list());
        else if (message.has_screen_type())
            emit sig_screenTypeChanged(message.screen_type());
        else
            LOG(WARNING) << "Unhandled screen message";
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_AUDIO)
    {
        proto::audio::HostToClient message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse audio message";
            return;
        }

        readAudioPacket(message.packet());
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_CONTROL)
    {
        proto::control::HostToClient message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse control message";
            return;
        }

        if (message.has_capabilities())
        {
            readCapabilities(message.capabilities());
        }
        else if (message.has_session_list())
        {
            CLOG(INFO) << "Received:" << message.session_list();
            emit sig_sessionListChanged(message.session_list());
        }
        else
        {
            // Unknown messages are ignored.
            CLOG(ERROR) << "Unhandled service message from host";
        }
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_CLIPBOARD)
    {
        proto::clipboard::HostToClient message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse clipboard message";
            return;
        }

        if (message.has_event())
            readClipboardEvent(message.event());
        else
            CLOG(ERROR) << "Unhandled clipboard message from host";
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_FILE)
    {
        proto::file::HostToClient message;
        if (!base::parse(buffer, &message))
        {
            LOG(ERROR) << "Unable to parse file message";
            return;
        }

        if (message.has_request())
        {
            // TODO
        }
        else if (message.has_data())
        {
            // TODO
        }
        else if (message.has_cancel())
        {
            // TODO
        }
        else if (message.has_error())
        {
            // TODO
        }
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_TASK_MANAGER)
    {
        proto::task_manager::HostToClient message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse task manager data";
            return;
        }

        emit sig_taskManager(message);
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_SYSTEM_INFO)
    {
        proto::system_info::SystemInfo message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse system info extension data";
            return;
        }

        emit sig_systemInfo(message);
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_LEGACY)
    {
        proto::legacy::SessionToClient message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Invalid session message from host";
            return;
        }

        if (message.has_video_packet() || message.has_cursor_shape())
        {
            if (message.has_video_packet())
                readVideoPacket(message.video_packet());
            if (message.has_cursor_shape())
                readCursorShape(message.cursor_shape());
        }
        else if (message.has_audio_packet())
        {
            readAudioPacket(message.audio_packet());
        }
        else if (message.has_cursor_position())
        {
            readCursorPosition(message.cursor_position());
        }
        else if (message.has_clipboard_event())
        {
            readClipboardEvent(message.clipboard_event());
        }
        else if (message.has_capabilities())
        {
            readCapabilities(message.capabilities());
        }
        else if (message.has_extension())
        {
            readExtension(message.extension());
        }
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onClipboardEvent(const proto::clipboard::Event& event)
{
    if (!input_event_filter_.sendClipboardEvent(event))
        return;

    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        message.mutable_clipboard_event()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, base::serialize(message));
    }
    else
    {
        proto::clipboard::ClientToHost message;
        message.mutable_event()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_CLIPBOARD, base::serialize(message));
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onDesktopConfigChanged(const proto::control::Config& config)
{
    desktop_config_ = config;

    if (!(desktop_config_.flags() & proto::control::ENABLE_CURSOR_SHAPE))
    {
        CLOG(INFO) << "Cursor shape disabled";
        cursor_decoder_.reset();
    }

    input_event_filter_.setClipboardEnabled(desktop_config_.flags() & proto::control::ENABLE_CLIPBOARD);

    CLOG(INFO) << "Send:" << config;

    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        message.mutable_config()->CopyFrom(desktop_config_);
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, base::serialize(message));
    }
    else
    {
        proto::control::ClientToHost message;
        message.mutable_config()->CopyFrom(desktop_config_);
        sendMessage(proto::desktop::CHANNEL_ID_CONTROL, base::serialize(message));
        sendSessionListRequest();
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onCurrentScreenChanged(const proto::screen::Screen& screen)
{
    CLOG(INFO) << "Current screen changed:" << screen.id();

    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        proto::legacy::Extension* extension = message.mutable_extension();
        extension->set_name(common::kSelectScreenExtension);
        extension->set_data(screen.SerializeAsString());
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, base::serialize(message));
    }
    else
    {
        proto::screen::ClientToHost message;
        message.mutable_screen()->CopyFrom(screen);
        sendMessage(proto::desktop::CHANNEL_ID_SCREEN, base::serialize(message));
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onPreferredSizeChanged(int width, int height)
{
    if (isLegacy())
        return;

    CLOG(INFO) << "Preferred size changed:" << width << "x" << height;

    proto::video::ClientToHost message;
    proto::video::PreferredSize* preferred_size = message.mutable_preferred_size();
    preferred_size->set_width(width);
    preferred_size->set_height(height);
    sendMessage(proto::desktop::CHANNEL_ID_VIDEO, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onVideoPauseChanged(bool enable)
{
    if (isLegacy())
        return;

    CLOG(INFO) << "Video pause changed:" << enable;

    if (enable)
    {
        ++video_pause_count_;
    }
    else
    {
        if (!video_pause_count_)
            return;
        ++video_resume_count_;
    }

    proto::video::ClientToHost message;
    proto::video::Pause* pause = message.mutable_pause();
    pause->set_enable(enable);
    pause->set_dummy(1);
    sendMessage(proto::desktop::CHANNEL_ID_VIDEO, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onAudioPauseChanged(bool enable)
{
    if (isLegacy())
        return;

    CLOG(INFO) << "Audio pause changed:" << enable;

    if (enable)
    {
        ++audio_pause_count_;
    }
    else
    {
        if (!audio_pause_count_)
            return;
        ++audio_resume_count_;
    }

    proto::audio::ClientToHost message;
    proto::audio::Pause* pause = message.mutable_pause();
    pause->set_enable(enable);
    pause->set_dummy(1);
    sendMessage(proto::desktop::CHANNEL_ID_AUDIO, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onRecordingChanged(bool enable, const QString& file_path)
{
    proto::user::ClientToHost message;
    proto::user::VideoRecording* video_recording = message.mutable_video_recording();

    if (enable)
    {
        CLOG(INFO) << "Video recording is enabled:" << file_path;

        video_recording->set_action(proto::user::VideoRecording::ACTION_STARTED);

        webm_file_writer_ =
            std::make_unique<base::WebmFileWriter>(file_path, sessionState()->computerName());
        webm_video_encoder_ = std::make_unique<base::WebmVideoEncoder>();

        webm_video_encode_timer_ = new QTimer(this);

        connect(webm_video_encode_timer_, &QTimer::timeout, this, [this]()
        {
            if (!webm_video_encoder_ || !webm_file_writer_ || !desktop_frame_)
                return;

            proto::video::Packet packet;

            if (webm_video_encoder_->encode(*desktop_frame_, &packet))
                webm_file_writer_->addVideoPacket(packet);
        });

        webm_video_encode_timer_->start(std::chrono::milliseconds(60));
    }
    else
    {
        CLOG(INFO) << "Video recording is disabled";

        video_recording->set_action(proto::user::VideoRecording::ACTION_STOPPED);

        if (webm_video_encode_timer_)
        {
            webm_video_encode_timer_->deleteLater();
            webm_video_encode_timer_ = nullptr;
        }
        webm_video_encoder_.reset();
        webm_file_writer_.reset();
    }

    if (isLegacy())
        return;

    sendMessage(proto::desktop::CHANNEL_ID_USER, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onKeyEvent(const proto::input::KeyEvent& event)
{
    if (!input_event_filter_.keyEvent(event))
        return;

    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        message.mutable_key_event()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, base::serialize(message));
    }
    else
    {
        proto::input::ClientToHost message;
        message.mutable_key()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_INPUT, base::serialize(message));
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onTextEvent(const proto::input::TextEvent& event)
{
    if (!input_event_filter_.textEvent(event))
        return;

    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        message.mutable_text_event()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, base::serialize(message));
    }
    else
    {
        proto::input::ClientToHost message;
        message.mutable_text()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_INPUT, base::serialize(message));
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onMouseEvent(const proto::input::MouseEvent& event)
{
    if (!input_event_filter_.mouseEvent(event))
        return;

    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        message.mutable_mouse_event()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, base::serialize(message));
    }
    else
    {
        proto::input::ClientToHost message;
        message.mutable_mouse()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_INPUT, base::serialize(message));
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onPowerControl(proto::power::Control::Action action)
{
    if (sessionState()->sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        CLOG(INFO) << "Power control supported only for desktop manage session";
        return;
    }

    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        proto::legacy::Extension* extension = message.mutable_extension();
        proto::power::Control power_control;
        power_control.set_action(action);
        extension->set_name(common::kPowerControlExtension);
        extension->set_data(power_control.SerializeAsString());

        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, base::serialize(message));
    }
    else
    {
        proto::power::ClientToHost message;
        message.mutable_power_control()->set_action(action);
        sendMessage(proto::desktop::CHANNEL_ID_POWER, base::serialize(message));
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request)
{
    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        proto::legacy::Extension* extension = message.mutable_extension();
        extension->set_name(common::kSystemInfoExtension);
        extension->set_data(request.SerializeAsString());
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, base::serialize(message));
    }
    else
    {
        sendMessage(proto::desktop::CHANNEL_ID_SYSTEM_INFO, base::serialize(request));
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onTaskManager(const proto::task_manager::ClientToHost& task_manager)
{
    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        proto::legacy::Extension* extension = message.mutable_extension();
        extension->set_name(common::kTaskManagerExtension);
        extension->set_data(task_manager.SerializeAsString());
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, base::serialize(message));
    }
    else
    {
        sendMessage(proto::desktop::CHANNEL_ID_TASK_MANAGER, base::serialize(task_manager));
    }
}

//--------------------------------------------------------------------------------------------------
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

    Metrics metrics;
    metrics.duration = session_duration;
    metrics.total_tcp_rx = totalTcpRx();
    metrics.total_tcp_tx = totalTcpTx();
    metrics.speed_tcp_rx = speedTcpRx();
    metrics.speed_tcp_tx = speedTcpTx();
    metrics.total_udp_rx = totalUdpRx();
    metrics.total_udp_tx = totalUdpTx();
    metrics.speed_udp_rx = speedUdpRx();
    metrics.speed_udp_tx = speedUdpTx();

    if (min_video_packet_ != std::numeric_limits<size_t>::max())
        metrics.min_video_packet = min_video_packet_;

    metrics.max_video_packet = max_video_packet_;
    metrics.avg_video_packet = avg_video_packet_;
    metrics.video_packet_count = video_packet_count_;
    metrics.video_pause_count = video_pause_count_;
    metrics.video_resume_count = video_resume_count_;

    if (min_audio_packet_ != std::numeric_limits<size_t>::max())
        metrics.min_audio_packet = min_audio_packet_;

    metrics.max_audio_packet = max_audio_packet_;
    metrics.avg_audio_packet = avg_audio_packet_;
    metrics.audio_packet_count = audio_packet_count_;
    metrics.audio_pause_count = audio_pause_count_;
    metrics.audio_resume_count = audio_resume_count_;

    metrics.video_capturer_type = video_capturer_type_;
    metrics.fps = fps_;
    metrics.send_mouse = input_event_filter_.sendMouseCount();
    metrics.drop_mouse = input_event_filter_.dropMouseCount();
    metrics.send_key   = input_event_filter_.sendKeyCount();
    metrics.send_text  = input_event_filter_.sendTextCount();
    metrics.read_clipboard = input_event_filter_.readClipboardCount();
    metrics.send_clipboard = input_event_filter_.sendClipboardCount();
    metrics.cursor_shape_count = cursor_shape_count_;
    metrics.cursor_pos_count   = cursor_pos_count_;

    if (cursor_decoder_)
    {
        metrics.cursor_cached = cursor_decoder_->cachedCursors();
        metrics.cursor_taken_from_cache = cursor_decoder_->takenCursorsFromCache();
    }

    emit sig_metrics(metrics);
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onSwitchSession(quint32 session_id)
{
    proto::control::ClientToHost message;
    proto::control::SwitchSession* switch_session = message.mutable_switch_session();
    switch_session->set_session_id(session_id);

    CLOG(INFO) << "Send:" << *switch_session;
    sendMessage(proto::desktop::CHANNEL_ID_CONTROL, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readCapabilities(const proto::control::Capabilities& capabilities)
{
    // We notify the window about changes in the list of extensions and video encodings.
    // A window can disable/enable some of its capabilities in accordance with this information.
    CLOG(INFO) << "Received:" << capabilities;
    emit sig_capabilities(capabilities);
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readVideoPacket(const proto::video::Packet& packet)
{
    proto::video::ErrorCode error_code = packet.error_code();
    if (error_code != proto::video::ERROR_CODE_OK)
    {
        CLOG(ERROR) << "Video error detected:" << error_code;
        emit sig_frameError(error_code);
        return;
    }

    if (video_encoding_ != packet.encoding())
    {
        CLOG(INFO) << "Video encoding changed from" << video_encoding_ << "to" << packet.encoding();
        video_decoder_ = std::make_unique<base::VideoDecoder>(packet.encoding());
        video_encoding_ = packet.encoding();

        if (desktop_config_.video_encoding() != video_encoding_)
        {
            // The host can change the encoding at its own discretion.
            desktop_config_.set_video_encoding(video_encoding_);
            emit sig_videoEncodingChanged(video_encoding_);
        }
    }

    if (!video_decoder_)
    {
        CLOG(ERROR) << "Video decoder is not initialized";
        return;
    }

    if (packet.has_format())
    {
        const proto::video::PacketFormat& format = packet.format();

        CLOG(INFO) << "Video packet has format:" << format;
        key_frame_received_ = true;

        QSize video_size = QSize(format.video_rect().width(), format.video_rect().height());
        QSize screen_size = video_size;

        static const int kMaxValue = std::numeric_limits<quint16>::max();

        if (video_size.width()  <= 0 || video_size.width()  >= kMaxValue ||
            video_size.height() <= 0 || video_size.height() >= kMaxValue)
        {
            CLOG(ERROR) << "Wrong video frame size:" << video_size;
            return;
        }

        if (format.has_screen_size())
        {
            screen_size = QSize(format.screen_size().width(), format.screen_size().height());

            if (screen_size.width() <= 0 || screen_size.width() >= kMaxValue ||
                screen_size.height() <= 0 || screen_size.height() >= kMaxValue)
            {
                CLOG(ERROR) << "Wrong screen size:" << screen_size;
                return;
            }
        }

        video_capturer_type_ = format.capturer_type();
        desktop_frame_ = base::FrameAligned::create(video_size, 32);

        emit sig_frameChanged(screen_size, desktop_frame_);
    }

    if (packet.flags() & proto::video::PACKET_FLAG_IS_KEY_FRAME)
        key_frame_received_ = true;

    if (!key_frame_received_)
    {
        // Until a keyframe is not received, we drop all video packets.
        CLOG(INFO) << "Video packet is dropped";
        return;
    }

    if (!desktop_frame_)
    {
        CLOG(ERROR) << "The desktop frame is not initialized";
        return;
    }

    if (!video_decoder_->decode(packet, desktop_frame_.get()))
    {
        CLOG(ERROR) << "Unable to decode video packet";
        key_frame_received_ = false;
        sendKeyFrameRequest();
        return;
    }

    ++video_packet_count_;
    ++fps_frame_count_;

    size_t packet_size = packet.ByteSizeLong();

    avg_video_packet_ = calculateAvgSize(avg_video_packet_, packet_size);
    min_video_packet_ = std::min(min_video_packet_, packet_size);
    max_video_packet_ = std::max(max_video_packet_, packet_size);

    emit sig_drawFrame();
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readAudioPacket(const proto::audio::Packet& packet)
{
    if (webm_file_writer_)
        webm_file_writer_->addAudioPacket(packet);

    if (!audio_player_)
    {
        CLOG(ERROR) << "Audio packet received but audio player not initialized";
        return;
    }

    if (packet.encoding() != audio_encoding_)
    {
        if (packet.encoding() != proto::audio::ENCODING_OPUS)
        {
            CLOG(WARNING) << "Unsupported audio encoding:" << packet.encoding();
            return;
        }
        audio_decoder_ = std::make_unique<base::AudioDecoder>();
        audio_encoding_ = packet.encoding();

        CLOG(INFO) << "Audio encoding changed to:" << audio_encoding_;
    }

    if (!audio_decoder_)
    {
        CLOG(INFO) << "Audio decoder not initialized now";
        return;
    }

    size_t packet_size = packet.ByteSizeLong();

    avg_audio_packet_ = calculateAvgSize(avg_audio_packet_, packet_size);
    min_audio_packet_ = std::min(min_audio_packet_, packet_size);
    max_audio_packet_ = std::max(max_audio_packet_, packet_size);

    ++audio_packet_count_;

    std::unique_ptr<proto::audio::Packet> decoded_packet = audio_decoder_->decode(packet);
    if (decoded_packet)
        audio_player_->addPacket(std::move(decoded_packet));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readCursorShape(const proto::cursor::Shape& shape)
{
    if (sessionState()->sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        CLOG(ERROR) << "Cursor shape received not session type not desktop manage";
        return;
    }

    if (!(desktop_config_.flags() & proto::control::ENABLE_CURSOR_SHAPE))
    {
        CLOG(ERROR) << "Cursor shape received not disabled in client";
        return;
    }

    ++cursor_shape_count_;

    if (!cursor_decoder_)
    {
        CLOG(INFO) << "Cursor decoder initialization";
        cursor_decoder_ = std::make_unique<base::CursorDecoder>();
    }

    std::shared_ptr<base::MouseCursor> mouse_cursor = cursor_decoder_->decode(shape);
    if (!mouse_cursor)
        return;

    emit sig_mouseCursorChanged(std::move(mouse_cursor));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readCursorPosition(const proto::cursor::Position& position)
{
    if (!(desktop_config_.flags() & proto::control::CURSOR_POSITION))
        return;

    ++cursor_pos_count_;
    emit sig_cursorPositionChanged(position);
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readClipboardEvent(const proto::clipboard::Event& event)
{
    if (clipboard_monitor_ && input_event_filter_.readClipboardEvent(event))
        clipboard_monitor_->injectClipboardEvent(event);
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readExtension(const proto::legacy::Extension& extension)
{
    if (!isLegacy())
        return;

    if (extension.name() == common::kTaskManagerExtension)
    {
        proto::task_manager::HostToClient message;
        if (!message.ParseFromString(extension.data()))
        {
            CLOG(ERROR) << "Unable to parse task manager extension data";
            return;
        }

        emit sig_taskManager(message);
    }
    else if (extension.name() == common::kSelectScreenExtension)
    {
        proto::screen::ScreenList screen_list;
        if (!screen_list.ParseFromString(extension.data()))
        {
            CLOG(ERROR) << "Unable to parse select screen extension data";
            return;
        }

        CLOG(INFO) << "Screen list receive:" << screen_list;
        emit sig_screenListChanged(screen_list);
    }
    else if (extension.name() == common::kScreenTypeExtension)
    {
        proto::screen::ScreenType screen_type;
        if (!screen_type.ParseFromString(extension.data()))
        {
            CLOG(ERROR) << "Unable to parse screen type extension data";
            return;
        }

        CLOG(INFO) << "Screen type received:" << screen_type;
        emit sig_screenTypeChanged(screen_type);
    }
    else if (extension.name() == common::kSystemInfoExtension)
    {
        proto::system_info::SystemInfo system_info;
        if (!system_info.ParseFromString(extension.data()))
        {
            CLOG(ERROR) << "Unable to parse system info extension data";
            return;
        }

        emit sig_systemInfo(system_info);
    }
    else
    {
        CLOG(ERROR) << "Unknown extension:" << extension.name();
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::sendSessionListRequest()
{
    proto::control::ClientToHost message;
    proto::control::SessionListRequest* request = message.mutable_session_list_request();
    request->set_dummy(1);
    sendMessage(proto::desktop::CHANNEL_ID_CONTROL, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::sendKeyFrameRequest()
{
    if (isLegacy())
        return;

    proto::video::ClientToHost message;
    message.mutable_key_frame()->set_dummy(1);
    sendMessage(proto::desktop::CHANNEL_ID_VIDEO, base::serialize(message));
}

} // namespace client
