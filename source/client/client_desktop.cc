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
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientDesktop::~ClientDesktop()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onSessionStarted()
{
    LOG(INFO) << "Desktop session started";

    start_time_ = Clock::now();
    key_frame_received_ = false;
    started_ = true;

    input_event_filter_.setSessionType(sessionState()->sessionType());

    clipboard_monitor_ = new common::ClipboardMonitor(this);
    connect(clipboard_monitor_, &common::ClipboardMonitor::sig_clipboardEvent,
            this, &ClientDesktop::onClipboardEvent);
    clipboard_monitor_->start();

    audio_player_ = base::AudioPlayer::create();
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onSessionMessageReceived(const QByteArray& buffer)
{
    if (!incoming_message_.parse(buffer))
    {
        LOG(ERROR) << "Invalid session message from host";
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
    else if (incoming_message_->has_cursor_position())
    {
        readCursorPosition(incoming_message_->cursor_position());
    }
    else if (incoming_message_->has_clipboard_event())
    {
        readClipboardEvent(incoming_message_->clipboard_event());
    }
    else if (incoming_message_->has_capabilities())
    {
        readCapabilities(incoming_message_->capabilities());
    }
    else if (incoming_message_->has_extension())
    {
        readExtension(incoming_message_->extension());
    }
    else
    {
        // Unknown messages are ignored.
        LOG(ERROR) << "Unhandled session message from host";
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onServiceMessageReceived(const QByteArray& buffer)
{
    proto::desktop::ServiceToClient message;

    if (!base::parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse service message";
        return;
    }

    if (message.has_session_list())
    {
        LOG(INFO) << "Received:" << message.session_list();
        emit sig_sessionListChanged(message.session_list());
    }
    else
    {
        // Unknown messages are ignored.
        LOG(ERROR) << "Unhandled service message from host";
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (!input_event_filter_.sendClipboardEvent(event))
        return;

    outgoing_message_.newMessage().mutable_clipboard_event()->CopyFrom(event);
    sendSessionMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onDesktopConfigChanged(const proto::desktop::Config& config)
{
    desktop_config_ = config;

    if (!(desktop_config_.flags() & proto::desktop::ENABLE_CURSOR_SHAPE))
    {
        LOG(INFO) << "Cursor shape disabled";
        cursor_decoder_.reset();
    }

    input_event_filter_.setClipboardEnabled(desktop_config_.flags() & proto::desktop::ENABLE_CLIPBOARD);

    LOG(INFO) << "Send:" << config;
    outgoing_message_.newMessage().mutable_config()->CopyFrom(desktop_config_);
    sendSessionMessage(outgoing_message_.serialize());
    sendSessionListRequest();
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onCurrentScreenChanged(const proto::desktop::Screen& screen)
{
    LOG(INFO) << "Current screen changed:" << screen.id();

    proto::desktop::Extension* extension = outgoing_message_.newMessage().mutable_extension();
    extension->set_name(common::kSelectScreenExtension);
    extension->set_data(screen.SerializeAsString());

    sendSessionMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onPreferredSizeChanged(int width, int height)
{
    LOG(INFO) << "Preferred size changed:" << width << "x" << height;

    proto::desktop::Size preferred_size;
    preferred_size.set_width(width);
    preferred_size.set_height(height);

    proto::desktop::Extension* extension = outgoing_message_.newMessage().mutable_extension();
    extension->set_name(common::kPreferredSizeExtension);
    extension->set_data(preferred_size.SerializeAsString());

    sendSessionMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onVideoPauseChanged(bool enable)
{
    LOG(INFO) << "Video pause changed:" << enable;

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

    proto::desktop::Pause pause;
    pause.set_enable(enable);

    proto::desktop::Extension* extension = outgoing_message_.newMessage().mutable_extension();
    extension->set_name(common::kVideoPauseExtension);
    extension->set_data(pause.SerializeAsString());

    sendSessionMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onAudioPauseChanged(bool enable)
{
    LOG(INFO) << "Audio pause changed:" << enable;

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

    proto::desktop::Pause pause;
    pause.set_enable(enable);

    proto::desktop::Extension* extension = outgoing_message_.newMessage().mutable_extension();
    extension->set_name(common::kAudioPauseExtension);
    extension->set_data(pause.SerializeAsString());

    sendSessionMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onRecordingChanged(bool enable, const QString& file_path)
{
    proto::desktop::ClientToService message;
    proto::desktop::VideoRecording* video_recording = message.mutable_video_recording();

    if (enable)
    {
        LOG(INFO) << "Video recording is enabled:" << file_path;

        video_recording->set_action(proto::desktop::VideoRecording::ACTION_STARTED);

        webm_file_writer_ =
            std::make_unique<base::WebmFileWriter>(file_path, sessionState()->computerName());
        webm_video_encoder_ = std::make_unique<base::WebmVideoEncoder>();

        webm_video_encode_timer_ = new QTimer(this);

        connect(webm_video_encode_timer_, &QTimer::timeout, this, [this]()
        {
            if (!webm_video_encoder_ || !webm_file_writer_ || !desktop_frame_)
                return;

            proto::desktop::VideoPacket packet;

            if (webm_video_encoder_->encode(*desktop_frame_, &packet))
                webm_file_writer_->addVideoPacket(packet);
        });

        webm_video_encode_timer_->start(std::chrono::milliseconds(60));
    }
    else
    {
        LOG(INFO) << "Video recording is disabled";

        video_recording->set_action(proto::desktop::VideoRecording::ACTION_STOPPED);

        if (webm_video_encode_timer_)
        {
            webm_video_encode_timer_->deleteLater();
            webm_video_encode_timer_ = nullptr;
        }
        webm_video_encoder_.reset();
        webm_file_writer_.reset();
    }

    sendServiceMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onKeyEvent(const proto::desktop::KeyEvent& event)
{
    if (!input_event_filter_.keyEvent(event))
        return;

    outgoing_message_.newMessage().mutable_key_event()->CopyFrom(event);
    sendSessionMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onTextEvent(const proto::desktop::TextEvent& event)
{
    if (!input_event_filter_.textEvent(event))
        return;

    outgoing_message_.newMessage().mutable_text_event()->CopyFrom(event);
    sendSessionMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onMouseEvent(const proto::desktop::MouseEvent& event)
{
    if (!input_event_filter_.mouseEvent(event))
        return;

    outgoing_message_.newMessage().mutable_mouse_event()->CopyFrom(event);
    sendSessionMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onPowerControl(proto::desktop::PowerControl::Action action)
{
    if (sessionState()->sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(INFO) << "Power control supported only for desktop manage session";
        return;
    }

    proto::desktop::Extension* extension = outgoing_message_.newMessage().mutable_extension();
    proto::desktop::PowerControl power_control;
    power_control.set_action(action);
    extension->set_name(common::kPowerControlExtension);
    extension->set_data(power_control.SerializeAsString());

    sendSessionMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onRemoteUpdate()
{
    outgoing_message_.newMessage().mutable_extension()->set_name(common::kRemoteUpdateExtension);
    sendSessionMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request)
{
    proto::desktop::Extension* extension = outgoing_message_.newMessage().mutable_extension();
    extension->set_name(common::kSystemInfoExtension);
    extension->set_data(request.SerializeAsString());
    sendSessionMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onTaskManager(const proto::task_manager::ClientToHost& message)
{
    proto::desktop::Extension* extension = outgoing_message_.newMessage().mutable_extension();
    extension->set_name(common::kTaskManagerExtension);
    extension->set_data(message.SerializeAsString());
    sendSessionMessage(outgoing_message_.serialize());
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
    proto::desktop::ClientToService message;
    proto::desktop::SwitchSession* switch_session = message.mutable_switch_session();
    switch_session->set_session_id(session_id);

    LOG(INFO) << "Send:" << *switch_session;
    sendServiceMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readCapabilities(const proto::desktop::Capabilities& capabilities)
{
    // We notify the window about changes in the list of extensions and video encodings.
    // A window can disable/enable some of its capabilities in accordance with this information.
    LOG(INFO) << "Received:" << capabilities;
    emit sig_capabilities(capabilities);
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readVideoPacket(const proto::desktop::VideoPacket& packet)
{
    proto::desktop::VideoErrorCode error_code = packet.error_code();
    if (error_code != proto::desktop::VIDEO_ERROR_CODE_OK)
    {
        LOG(ERROR) << "Video error detected:" << error_code;
        emit sig_frameError(error_code);
        return;
    }

    if (video_encoding_ != packet.encoding())
    {
        LOG(INFO) << "Video encoding changed from" << video_encoding_ << "to" << packet.encoding();
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
        LOG(ERROR) << "Video decoder is not initialized";
        return;
    }

    if (packet.has_format())
    {
        const proto::desktop::VideoPacketFormat& format = packet.format();

        LOG(INFO) << "Video packet has format:" << format;
        key_frame_received_ = true;

        QSize video_size = QSize(format.video_rect().width(), format.video_rect().height());
        QSize screen_size = video_size;

        static const int kMaxValue = std::numeric_limits<quint16>::max();

        if (video_size.width()  <= 0 || video_size.width()  >= kMaxValue ||
            video_size.height() <= 0 || video_size.height() >= kMaxValue)
        {
            LOG(ERROR) << "Wrong video frame size:" << video_size;
            return;
        }

        if (format.has_screen_size())
        {
            screen_size = base::parse(format.screen_size());

            if (screen_size.width() <= 0 || screen_size.width() >= kMaxValue ||
                screen_size.height() <= 0 || screen_size.height() >= kMaxValue)
            {
                LOG(ERROR) << "Wrong screen size:" << screen_size;
                return;
            }
        }

        video_capturer_type_ = format.capturer_type();
        desktop_frame_ = base::FrameAligned::create(video_size, 32);

        emit sig_frameChanged(screen_size, desktop_frame_);
    }

    if (packet.flags() & proto::desktop::VIDEO_PACKET_FLAG_IS_KEY_FRAME)
        key_frame_received_ = true;

    if (!key_frame_received_)
    {
        // Until a keyframe is not received, we drop all video packets.
        LOG(INFO) << "Video packet is dropped";
        return;
    }

    if (!desktop_frame_)
    {
        LOG(ERROR) << "The desktop frame is not initialized";
        return;
    }

    if (!video_decoder_->decode(packet, desktop_frame_.get()))
    {
        LOG(ERROR) << "Unable to decode video packet";
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
void ClientDesktop::readAudioPacket(const proto::desktop::AudioPacket& packet)
{
    if (webm_file_writer_)
        webm_file_writer_->addAudioPacket(packet);

    if (!audio_player_)
    {
        LOG(ERROR) << "Audio packet received but audio player not initialized";
        return;
    }

    if (packet.encoding() != audio_encoding_)
    {
        if (packet.encoding() != proto::desktop::AUDIO_ENCODING_OPUS)
        {
            LOG(WARNING) << "Unsupported audio encoding:" << packet.encoding();
            return;
        }
        audio_decoder_ = std::make_unique<base::AudioDecoder>();
        audio_encoding_ = packet.encoding();

        LOG(INFO) << "Audio encoding changed to:" << audio_encoding_;
    }

    if (!audio_decoder_)
    {
        LOG(INFO) << "Audio decoder not initialized now";
        return;
    }

    size_t packet_size = packet.ByteSizeLong();

    avg_audio_packet_ = calculateAvgSize(avg_audio_packet_, packet_size);
    min_audio_packet_ = std::min(min_audio_packet_, packet_size);
    max_audio_packet_ = std::max(max_audio_packet_, packet_size);

    ++audio_packet_count_;

    std::unique_ptr<proto::desktop::AudioPacket> decoded_packet = audio_decoder_->decode(packet);
    if (decoded_packet)
        audio_player_->addPacket(std::move(decoded_packet));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readCursorShape(const proto::desktop::CursorShape& cursor_shape)
{
    if (sessionState()->sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(ERROR) << "Cursor shape received not session type not desktop manage";
        return;
    }

    if (!(desktop_config_.flags() & proto::desktop::ENABLE_CURSOR_SHAPE))
    {
        LOG(ERROR) << "Cursor shape received not disabled in client";
        return;
    }

    ++cursor_shape_count_;

    if (!cursor_decoder_)
    {
        LOG(INFO) << "Cursor decoder initialization";
        cursor_decoder_ = std::make_unique<base::CursorDecoder>();
    }

    std::shared_ptr<base::MouseCursor> mouse_cursor = cursor_decoder_->decode(cursor_shape);
    if (!mouse_cursor)
        return;

    emit sig_mouseCursorChanged(std::move(mouse_cursor));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readCursorPosition(const proto::desktop::CursorPosition& cursor_position)
{
    if (!(desktop_config_.flags() & proto::desktop::CURSOR_POSITION))
        return;

    ++cursor_pos_count_;
    emit sig_cursorPositionChanged(cursor_position);
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (clipboard_monitor_ && input_event_filter_.readClipboardEvent(event))
        clipboard_monitor_->injectClipboardEvent(event);
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readExtension(const proto::desktop::Extension& extension)
{
    if (extension.name() == common::kTaskManagerExtension)
    {
        proto::task_manager::HostToClient message;

        if (!message.ParseFromString(extension.data()))
        {
            LOG(ERROR) << "Unable to parse task manager extension data";
            return;
        }

        emit sig_taskManager(message);
    }
    else if (extension.name() == common::kSelectScreenExtension)
    {
        proto::desktop::ScreenList screen_list;

        if (!screen_list.ParseFromString(extension.data()))
        {
            LOG(ERROR) << "Unable to parse select screen extension data";
            return;
        }

        LOG(INFO) << "Screen list receive:" << screen_list;
        emit sig_screenListChanged(screen_list);
    }
    else if (extension.name() == common::kScreenTypeExtension)
    {
        proto::desktop::ScreenType screen_type;

        if (!screen_type.ParseFromString(extension.data()))
        {
            LOG(ERROR) << "Unable to parse screen type extension data";
            return;
        }

        LOG(INFO) << "Screen type received:" << screen_type;
        emit sig_screenTypeChanged(screen_type);
    }
    else if (extension.name() == common::kSystemInfoExtension)
    {
        proto::system_info::SystemInfo system_info;

        if (!system_info.ParseFromString(extension.data()))
        {
            LOG(ERROR) << "Unable to parse system info extension data";
            return;
        }

        emit sig_systemInfo(system_info);
    }
    else
    {
        LOG(ERROR) << "Unknown extension:" << extension.name();
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::sendSessionListRequest()
{
    proto::desktop::ClientToService message;
    proto::desktop::SessionListRequest* request = message.mutable_session_list_request();
    request->set_dummy(1);
    sendServiceMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::sendKeyFrameRequest()
{
    proto::desktop::Extension* extension = outgoing_message_.newMessage().mutable_extension();
    extension->set_name(common::kKeyFrameExtension);
    sendSessionMessage(outgoing_message_.serialize());
}

} // namespace client
