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
#include "base/serialization.h"
#include "base/codec/cursor_decoder.h"
#include "base/codec/webm_file_writer.h"
#include "base/desktop/frame.h"
#include "base/desktop/mouse_cursor.h"
#include "base/threading/worker_manager.h"
#include "client/workers/audio_worker.h"
#include "client/workers/video_worker.h"
#include "common/desktop_session_constants.h"
#include "proto/desktop_audio.h"
#include "proto/desktop_channel.h"
#include "proto/desktop_clipboard.h"
#include "proto/desktop_cursor.h"
#include "proto/desktop_input.h"
#include "proto/desktop_legacy.h"
#include "proto/desktop_screen.h"
#include "proto/desktop_user.h"
#include "proto/desktop_video.h"
#include "proto/task_manager.h"

namespace {

//--------------------------------------------------------------------------------------------------
int calculateFps(int last_fps, const std::chrono::milliseconds& duration, qint64 count)
{
    static const double kAlpha = 0.1;
    const qint64 ms = duration.count();
    if (ms <= 0)
        return last_fps;
    return int((kAlpha * ((1000.0 / double(ms)) * double(count))) + ((1.0 - kAlpha) * double(last_fps)));
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
ClientDesktop::ClientDesktop(const proto::control::Config& desktop_config, QObject* parent)
    : Client(parent),
      repeated_timer_(new QTimer(this)),
      desktop_config_(desktop_config)
{
    CLOG(INFO) << "Ctor";

    repeated_timer_->setInterval(1000);
    connect(repeated_timer_, &QTimer::timeout, this, &ClientDesktop::onRepeatedTimer);
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
    started_ = true;

    repeated_timer_->start();

    // The clipboard itself lives on the GUI thread and is wired to this client by the GUI. Only the
    // file transfer, which sends/receives over the network, stays here.
    clipboard_file_transfer_ = new ClipboardFileTransfer(this);

    connect(clipboard_file_transfer_, &ClipboardFileTransfer::sig_sendMessage,
            this, [this](const QByteArray& buffer)
    {
        sendMessage(proto::desktop::CHANNEL_ID_FILE, buffer);
    });

    connect(clipboard_file_transfer_, &ClipboardFileTransfer::sig_fileDataChunk,
            this, &ClientDesktop::sig_clipboardFileData);

    worker_manager_ = std::make_unique<WorkerManager>();

    std::unique_ptr<AudioWorker> audio_worker = std::make_unique<AudioWorker>();
    audio_worker_ = audio_worker.get();
    worker_manager_->add(std::move(audio_worker));

    std::unique_ptr<VideoWorker> video_worker = std::make_unique<VideoWorker>();
    video_worker_ = video_worker.get();
    worker_manager_->add(std::move(video_worker));

    connect(this, &ClientDesktop::sig_audioPacket, audio_worker_, &AudioWorker::onAudioPacket,
            Qt::QueuedConnection);
    connect(this, &ClientDesktop::sig_videoPacket, video_worker_, &VideoWorker::onVideoPacket,
            Qt::QueuedConnection);
    connect(this, &ClientDesktop::sig_videoRecording, video_worker_, &VideoWorker::onSetRecording,
            Qt::QueuedConnection);

    connect(video_worker_, &VideoWorker::sig_frameError, this, &ClientDesktop::sig_frameError,
            Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_frameChanged, this, &ClientDesktop::sig_frameChanged,
            Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_drawFrame, this, &ClientDesktop::onVideoDrawFrame,
            Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_videoInfoChanged, this, &ClientDesktop::onVideoInfoChanged,
            Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_keyFrameRequired, this, &ClientDesktop::onVideoKeyFrameRequired,
            Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_temporaryError, this, &ClientDesktop::onVideoTemporaryError,
            Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_h264Disabled, this, &ClientDesktop::onVideoH264Disabled,
            Qt::QueuedConnection);
    connect(video_worker_, &VideoWorker::sig_recordingVideoPacket, this, &ClientDesktop::onRecordingVideoPacket,
            Qt::QueuedConnection);

    worker_manager_->start();

    if (isLegacy())
        return;

    sendCapabilities();
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::desktop::CHANNEL_ID_VIDEO)
    {
        proto::video::HostToClient* message =
            incoming_message_.parse<proto::video::HostToClient>(buffer);
        if (!message)
        {
            CLOG(ERROR) << "Unable to parse video message";
            return;
        }

        if (message->has_packet())
            readVideoPacket(message->packet());
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_CURSOR)
    {
        proto::cursor::HostToClient* message =
            incoming_message_.parse<proto::cursor::HostToClient>(buffer);
        if (!message)
        {
            CLOG(ERROR) << "Unable to parse cursor message";
            return;
        }

        if (message->has_shape())
            readCursorShape(message->shape());
        else if (message->has_position())
            readCursorPosition(message->position());
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_SCREEN)
    {
        proto::screen::HostToClient* message =
            incoming_message_.parse<proto::screen::HostToClient>(buffer);
        if (!message)
        {
            CLOG(ERROR) << "Unable to parse screen message";
            return;
        }

        if (message->has_screen_list())
            emit sig_screenListChanged(message->screen_list());
        else if (message->has_screen_type())
            emit sig_screenTypeChanged(message->screen_type());
        else
            LOG(WARNING) << "Unhandled screen message";
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_AUDIO)
    {
        proto::audio::HostToClient* message =
            incoming_message_.parse<proto::audio::HostToClient>(buffer);
        if (!message)
        {
            CLOG(ERROR) << "Unable to parse audio message";
            return;
        }

        readAudioPacket(message->packet());
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_CONTROL)
    {
        proto::control::HostToClient message;
        if (!parse(buffer, &message))
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
        if (!parse(buffer, &message))
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
        if (clipboard_file_transfer_)
            clipboard_file_transfer_->onIncomingMessage(buffer);
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_TASK_MANAGER)
    {
        proto::task_manager::HostToClient message;
        if (!parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse task manager data";
            return;
        }

        emit sig_taskManager(message);
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_LEGACY)
    {
        proto::legacy::SessionToClient message;
        if (!parse(buffer, &message))
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
            readLegacyCapabilities(message.capabilities());
        }
        else if (message.has_extension())
        {
            readExtension(message.extension());
        }
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onDesktopConfigChanged(const proto::control::Config& config)
{
    desktop_config_ = config;

    if (!desktop_config_.cursor_shape())
    {
        CLOG(INFO) << "Cursor shape disabled";
        cursor_decoder_.reset();
    }

    sendConfig(desktop_config_);
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onCurrentScreenChanged(const proto::screen::Screen& screen)
{
    CLOG(INFO) << "Current screen changed:" << screen.id();

    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        proto::legacy::Extension* extension = message.mutable_extension();
        extension->set_name(kSelectScreenExtension);
        extension->set_data(screen.SerializeAsString());
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, serialize(message));
    }
    else
    {
        proto::screen::ClientToHost message;
        message.mutable_screen()->CopyFrom(screen);
        sendMessage(proto::desktop::CHANNEL_ID_SCREEN, serialize(message));
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
    sendMessage(proto::desktop::CHANNEL_ID_VIDEO, serialize(message));
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

        webm_file_writer_ = std::make_unique<WebmFileWriter>(file_path, sessionState()->computerName());
    }
    else
    {
        CLOG(INFO) << "Video recording is disabled";

        video_recording->set_action(proto::user::VideoRecording::ACTION_STOPPED);

        webm_file_writer_.reset();
    }

    // Frames for the recording are encoded in the video worker and delivered back through
    // sig_recordingVideoPacket.
    emit sig_videoRecording(enable);

    if (isLegacy())
        return;

    sendMessage(proto::desktop::CHANNEL_ID_USER, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onKeyEvent(const proto::input::KeyEvent& event)
{
    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        message.mutable_key_event()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, serialize(message));
    }
    else
    {
        proto::input::ClientToHost& message = outgoing_message_.newMessage<proto::input::ClientToHost>();
        message.mutable_key()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_INPUT,
                    outgoing_message_.serialize<proto::input::ClientToHost>());
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onTextEvent(const proto::input::TextEvent& event)
{
    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        message.mutable_text_event()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, serialize(message));
    }
    else
    {
        proto::input::ClientToHost& message = outgoing_message_.newMessage<proto::input::ClientToHost>();
        message.mutable_text()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_INPUT,
                    outgoing_message_.serialize<proto::input::ClientToHost>());
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onMouseEvent(const proto::input::MouseEvent& event)
{
    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        message.mutable_mouse_event()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, serialize(message));
    }
    else
    {
        proto::input::ClientToHost& message = outgoing_message_.newMessage<proto::input::ClientToHost>();
        message.mutable_mouse()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_INPUT,
                    outgoing_message_.serialize<proto::input::ClientToHost>());
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onPowerControl(proto::power::Control::Action action)
{
    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        proto::legacy::Extension* extension = message.mutable_extension();
        proto::power::Control power_control;
        power_control.set_action(action);
        extension->set_name(kPowerControlExtension);
        extension->set_data(power_control.SerializeAsString());

        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, serialize(message));
    }
    else
    {
        proto::power::ClientToHost message;
        message.mutable_power_control()->set_action(action);
        sendMessage(proto::desktop::CHANNEL_ID_POWER, serialize(message));
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onTaskManager(const proto::task_manager::ClientToHost& task_manager)
{
    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        proto::legacy::Extension* extension = message.mutable_extension();
        extension->set_name(kTaskManagerExtension);
        extension->set_data(task_manager.SerializeAsString());
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, serialize(message));
    }
    else
    {
        sendMessage(proto::desktop::CHANNEL_ID_TASK_MANAGER, serialize(task_manager));
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
    metrics.udp_method = udpMethod();

    if (min_video_packet_ != std::numeric_limits<size_t>::max())
        metrics.min_video_packet = min_video_packet_;

    metrics.max_video_packet = max_video_packet_;
    metrics.avg_video_packet = avg_video_packet_;
    metrics.video_packet_count = video_packet_count_;

    if (min_audio_packet_ != std::numeric_limits<size_t>::max())
        metrics.min_audio_packet = min_audio_packet_;

    metrics.max_audio_packet = max_audio_packet_;
    metrics.avg_audio_packet = avg_audio_packet_;
    metrics.audio_packet_count = audio_packet_count_;

    metrics.video_capturer_type = video_capturer_type_;
    metrics.video_encoder_type = video_encoder_type_;
    metrics.video_decoder_hardware = video_decoder_hardware_;
    metrics.fps = fps_;
    metrics.read_clipboard = read_clipboard_count_;
    metrics.send_clipboard = send_clipboard_count_;
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
    sendMessage(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onClipboardEvent(const proto::clipboard::Event& event)
{
    if (!desktop_config_.clipboard())
        return;

    ++send_clipboard_count_;

    if (event.mime_type() == Clipboard::kMimeTypeFileList.toStdString() &&
        !file_clipboard_supported_)
    {
        CLOG(WARNING) << "File clipboard is not supported by remote side, skipping file list";
        return;
    }

    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        message.mutable_clipboard_event()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, serialize(message));
    }
    else
    {
        proto::clipboard::ClientToHost message;
        message.mutable_event()->CopyFrom(event);
        sendMessage(proto::desktop::CHANNEL_ID_CLIPBOARD, serialize(message));
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onClipboardLocalFileListChanged(const QVector<LocalFileEntry>& files)
{
    if (clipboard_file_transfer_)
        clipboard_file_transfer_->setLocalFileList(files);
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onClipboardFileDataRequest(int file_index)
{
    if (clipboard_file_transfer_)
        clipboard_file_transfer_->requestFileData(file_index);
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onRepeatedTimer()
{
    if (force_reliable_active_ && reliable_hold_seconds_ > 0)
    {
        --reliable_hold_seconds_;
        if (reliable_hold_seconds_ == 0 && reliable_disable_count_ < 2)
        {
            ++reliable_disable_count_;
            CLOG(INFO) << "Disabling force reliable (disable count:" << reliable_disable_count_ << ")";
            setForceReliable(false);
            force_reliable_active_ = false;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onVideoDrawFrame(const QList<QRect>& dirty_rects, size_t packet_size)
{
    ++video_packet_count_;
    ++fps_frame_count_;

    avg_video_packet_ = calculateAvgSize(avg_video_packet_, packet_size);
    min_video_packet_ = std::min(min_video_packet_, packet_size);
    max_video_packet_ = std::max(max_video_packet_, packet_size);

    emit sig_drawFrame(dirty_rects);
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onVideoInfoChanged(quint32 capturer_type, quint32 encoder_type, bool hardware_decoder)
{
    video_capturer_type_ = capturer_type;
    video_encoder_type_ = encoder_type;
    video_decoder_hardware_ = hardware_decoder;
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onVideoKeyFrameRequired()
{
    sendKeyFrameRequest();
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onVideoTemporaryError()
{
    sendKeyFrameRequest();

    if (!force_reliable_active_ && reliable_disable_count_ < 2)
    {
        CLOG(INFO) << "Enabling force reliable (disable count:" << reliable_disable_count_ << ")";
        setForceReliable(true);
        force_reliable_active_ = true;
    }
    reliable_hold_seconds_ = 60;
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onVideoH264Disabled()
{
    h264_sw_enabled_ = false;
    sendCapabilities();
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::onRecordingVideoPacket(std::shared_ptr<proto::video::Packet> packet)
{
    if (webm_file_writer_ && packet)
        webm_file_writer_->addVideoPacket(*packet);
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readLegacyCapabilities(const proto::legacy::Capabilities& legacy_capabilities)
{
    CLOG(INFO) << "Received:" << legacy_capabilities;

    proto::control::Capabilities capabilities;

    auto add_flag = [&capabilities](const char* name, bool value)
    {
        proto::control::Capabilities::Flag* flag = capabilities.add_flag();
        flag->set_name(name);
        flag->set_value(value);
    };

    if (legacy_capabilities.os_type() == proto::legacy::Capabilities::OS_TYPE_WINDOWS)
        add_flag(kFlagOSWindows, true);

    quint32 video_encodings = legacy_capabilities.video_encodings();
    if (video_encodings & proto::video::ENCODING_VP8)
        add_flag(kFlagVideoVP8, true);
    if (video_encodings & proto::video::ENCODING_VP9)
        add_flag(kFlagVideoVP9, true);

    quint32 audio_encodings = legacy_capabilities.audio_encodings();
    if (audio_encodings & proto::audio::ENCODING_OPUS)
        add_flag(kFlagAudioOpus, true);

    // Convert legacy "disable_*" flags (inverted logic) to new positive flags.
    // If a disable flag is absent in the legacy message, the feature is considered enabled.
    bool paste_as_keystrokes = true;
    bool clipboard = true;
    bool cursor_shape = true;
    bool cursor_position = true;
    bool desktop_effects = true;
    bool desktop_wallpaper = true;
    bool lock_at_disconnect = true;
    bool block_input = true;

    for (int i = 0; i < legacy_capabilities.flag_size(); ++i)
    {
        const proto::legacy::Capabilities::Flag& flag = legacy_capabilities.flag(i);
        const std::string& name = flag.name();
        bool value = flag.value();

        if (name == kFlagDisablePasteAsKeystrokes)
            paste_as_keystrokes = !value;
        else if (name == kFlagDisableClipboard)
            clipboard = !value;
        else if (name == kFlagDisableCursorShape)
            cursor_shape = !value;
        else if (name == kFlagDisableCursorPosition)
            cursor_position = !value;
        else if (name == kFlagDisableDesktopEffects)
            desktop_effects = !value;
        else if (name == kFlagDisableDesktopWallpaper)
            desktop_wallpaper = !value;
        else if (name == kFlagDisableLockAtDisconnect)
            lock_at_disconnect = !value;
        else if (name == kFlagDisableBlockInput)
            block_input = !value;
    }

    add_flag(kFlagPasteAsKeystrokes, paste_as_keystrokes);
    add_flag(kFlagClipboard, clipboard);
    add_flag(kFlagCursorShape, cursor_shape);
    add_flag(kFlagCursorPosition, cursor_position);
    add_flag(kFlagDesktopEffects, desktop_effects);
    add_flag(kFlagDesktopWallpaper, desktop_wallpaper);
    add_flag(kFlagLockAtDisconnect, lock_at_disconnect);
    add_flag(kFlagBlockInput, block_input);

    // Convert legacy extensions string (semicolon-separated) to new flags.
    QString extensions = QString::fromStdString(legacy_capabilities.extensions());
    QStringList extension_list = extensions.split(';', Qt::SkipEmptyParts);

    add_flag(kFlagSelectScreen, extension_list.contains(kSelectScreenExtension));
    add_flag(kFlagPowerControl, extension_list.contains(kPowerControlExtension));
    add_flag(kFlagTaskManager, extension_list.contains(kTaskManagerExtension));

    CLOG(INFO) << "Converted:" << capabilities;
    emit sig_capabilities(capabilities);
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readCapabilities(const proto::control::Capabilities& capabilities)
{
    // We notify the window about changes in the list of extensions and video encodings.
    // A window can disable/enable some of its capabilities in accordance with this information.
    CLOG(INFO) << "Received:" << capabilities;

    for (int i = 0; i < capabilities.flag_size(); ++i)
    {
        const proto::control::Capabilities::Flag& flag = capabilities.flag(i);
        if (flag.name() == kFlagFileClipboard)
        {
            file_clipboard_supported_ = flag.value();
            break;
        }
    }

    emit sig_capabilities(capabilities);
    sendConfig(desktop_config_);
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readVideoPacket(const proto::video::Packet& packet)
{
    // Decoding, YUV conversion and decoder error handling happen in the video worker.
    emit sig_videoPacket(std::make_shared<proto::video::Packet>(packet));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readAudioPacket(const proto::audio::Packet& packet)
{
    if (webm_file_writer_)
        webm_file_writer_->addAudioPacket(packet);

    size_t packet_size = packet.ByteSizeLong();

    avg_audio_packet_ = calculateAvgSize(avg_audio_packet_, packet_size);
    min_audio_packet_ = std::min(min_audio_packet_, packet_size);
    max_audio_packet_ = std::max(max_audio_packet_, packet_size);

    ++audio_packet_count_;

    // Decoding and playback happen in the audio worker.
    emit sig_audioPacket(std::make_shared<proto::audio::Packet>(packet));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readCursorShape(const proto::cursor::Shape& shape)
{
    if (!desktop_config_.cursor_shape())
    {
        CLOG(ERROR) << "Cursor shape received not disabled in client";
        return;
    }

    ++cursor_shape_count_;

    if (!cursor_decoder_)
    {
        CLOG(INFO) << "Cursor decoder initialization";
        cursor_decoder_ = std::make_unique<CursorDecoder>();
    }

    std::shared_ptr<MouseCursor> mouse_cursor = cursor_decoder_->decode(shape);
    if (!mouse_cursor)
        return;

    emit sig_mouseCursorChanged(std::move(mouse_cursor));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readCursorPosition(const proto::cursor::Position& position)
{
    if (!desktop_config_.cursor_position())
        return;

    ++cursor_pos_count_;
    emit sig_cursorPositionChanged(position);
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readClipboardEvent(const proto::clipboard::Event& event)
{
    if (event.mime_type() == Clipboard::kMimeTypeFileList.toStdString() &&
        !file_clipboard_supported_)
    {
        CLOG(WARNING) << "File clipboard is not supported by remote side, ignoring file list";
        return;
    }

    if (desktop_config_.clipboard())
    {
        ++read_clipboard_count_;
        emit sig_injectClipboardEvent(event);
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::readExtension(const proto::legacy::Extension& extension)
{
    if (!isLegacy())
        return;

    if (extension.name() == kTaskManagerExtension)
    {
        proto::task_manager::HostToClient message;
        if (!message.ParseFromString(extension.data()))
        {
            CLOG(ERROR) << "Unable to parse task manager extension data";
            return;
        }

        emit sig_taskManager(message);
    }
    else if (extension.name() == kSelectScreenExtension)
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
    else if (extension.name() == kScreenTypeExtension)
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
    else
    {
        CLOG(ERROR) << "Unknown extension:" << extension.name();
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::sendSessionListRequest()
{
    proto::control::ClientToHost message;
    proto::control::SessionsRequest* request = message.mutable_sessions_request();
    request->set_dummy(1);
    sendMessage(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::sendConfig(const proto::control::Config& config)
{
    CLOG(INFO) << "Send:" << config;

    if (isLegacy())
    {
        proto::legacy::ClientToSession message;
        proto::legacy::Config* legacy_config = message.mutable_config();

        quint32 flags = proto::legacy::NO_FLAGS;
        if (config.cursor_shape())
            flags |= proto::legacy::ENABLE_CURSOR_SHAPE;
        if (config.clipboard())
            flags |= proto::legacy::ENABLE_CLIPBOARD;
        if (!config.effects())
            flags |= proto::legacy::DISABLE_EFFECTS;
        if (!config.wallpaper())
            flags |= proto::legacy::DISABLE_WALLPAPER;
        if (config.block_input())
            flags |= proto::legacy::BLOCK_REMOTE_INPUT;
        if (config.lock_at_disconnect())
            flags |= proto::legacy::LOCK_AT_DISCONNECT;
        if (config.cursor_position())
            flags |= proto::legacy::CURSOR_POSITION;

        legacy_config->set_flags(flags);
        legacy_config->set_video_encoding(proto::video::ENCODING_VP8);
        legacy_config->set_audio_encoding(
            config.audio() ? proto::audio::ENCODING_OPUS : proto::audio::ENCODING_UNKNOWN);

        sendMessage(proto::desktop::CHANNEL_ID_LEGACY, serialize(message));
    }
    else
    {
        proto::control::ClientToHost message;
        message.mutable_config()->CopyFrom(desktop_config_);
        sendMessage(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
        sendSessionListRequest();
    }
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::sendKeyFrameRequest()
{
    if (isLegacy())
        return;

    proto::video::ClientToHost message;
    message.mutable_key_frame()->set_dummy(1);
    sendMessage(proto::desktop::CHANNEL_ID_VIDEO, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::sendCapabilities()
{
    proto::control::ClientToHost message;
    proto::control::Capabilities* capabilities = message.mutable_capabilities();

    auto add_flag = [capabilities](const char* name, bool value)
    {
        proto::control::Capabilities::Flag* flag = capabilities->add_flag();
        flag->set_name(name);
        flag->set_value(value);
    };

    add_flag(kFlagVideoVP8, true);
    add_flag(kFlagVideoVP9, true);
    if (h264_sw_enabled_)
        add_flag(kFlagVideoH264, true);
    add_flag(kFlagAudioOpus, true);

#if defined(Q_OS_WINDOWS) || defined(Q_OS_MACOS)
    add_flag(kFlagFileClipboard, true);
#endif

    sendMessage(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientDesktop::setForceReliable(bool enable)
{
    if (isLegacy())
        return;

    proto::control::ClientToHost message;
    proto::control::Feedback* feedback = message.mutable_feedback();
    feedback->set_command_name("reliable");
    feedback->set_boolean(enable);
    sendMessage(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
}
