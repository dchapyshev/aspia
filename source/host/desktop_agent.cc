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

#include "host/desktop_agent.h"

#include <QCoreApplication>
#include <QTimer>

#include "base/application.h"
#include "base/logging.h"
#include "base/power_controller.h"
#include "base/audio/audio_capturer_wrapper.h"
#include "base/codec/audio_encoder.h"
#include "base/codec/cursor_encoder.h"
#include "base/codec/scale_reducer.h"
#include "base/codec/video_encoder.h"
#include "base/desktop/desktop_resizer.h"
#include "base/desktop/desktop_environment.h"
#include "base/desktop/mouse_cursor.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "host/desktop_agent_client.h"
#include "host/system_settings.h"
#include "proto/desktop_internal.h"

#if defined(Q_OS_WINDOWS)
#include "base/desktop/screen_capturer_win.h"
#include "host/input_injector_win.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include "base/desktop/screen_capturer_x11.h"
#include "host/input_injector_x11.h"
#endif // defined(Q_OS_LINUX)

namespace host {

namespace {

constexpr int kDefaultScreenCaptureFps = 24;
constexpr int kMinScreenCaptureFps = 8;
constexpr int kMaxScreenCaptureFpsHighEnd = 30;
constexpr int kMaxScreenCaptureFpsLowEnd = 20;

//--------------------------------------------------------------------------------------------------
int defaultCaptureFps()
{
    if (!qEnvironmentVariableIsSet("ASPIA_DEFAULT_FPS"))
        return kDefaultScreenCaptureFps;

    bool ok = false;
    int default_fps = qEnvironmentVariableIntValue("ASPIA_DEFAULT_FPS", &ok);
    if (!ok)
        return kDefaultScreenCaptureFps;

    if (default_fps < 1 || default_fps > 60)
    {
        LOG(INFO) << "Environment variable contains an incorrect default FPS:" << default_fps;
        return kDefaultScreenCaptureFps;
    }

    LOG(INFO) << "Default FPS specified by environment variable";
    return default_fps;
}

//--------------------------------------------------------------------------------------------------
int maxCaptureFps()
{
    int max_capture_fps = kMaxScreenCaptureFpsHighEnd;

    bool max_fps_from_env = false;
    if (qEnvironmentVariableIsSet("ASPIA_MAX_FPS"))
    {
        bool ok = false;
        int max_fps = qEnvironmentVariableIntValue("ASPIA_MAX_FPS", &ok);
        if (ok)
        {
            LOG(INFO) << "Maximum FPS specified by environment variable";

            if (max_fps < 1 || max_fps > 60)
            {
                LOG(INFO) << "Environment variable contains an incorrect maximum FPS:" << max_fps;
            }
            else
            {
                max_capture_fps = max_fps;
                max_fps_from_env = true;
            }
        }
    }

    if (!max_fps_from_env)
    {
        quint32 threads = QThread::idealThreadCount();
        if (threads <= 2)
        {
            LOG(INFO) << "Low-end CPU detected. Maximum capture FPS:" << kMaxScreenCaptureFpsLowEnd;
            max_capture_fps = kMaxScreenCaptureFpsLowEnd;
        }
    }

    return max_capture_fps;
}

//--------------------------------------------------------------------------------------------------
int minCaptureFps()
{
    if (!qEnvironmentVariableIsSet("ASPIA_MIN_FPS"))
        return kMinScreenCaptureFps;

    bool ok = false;
    int min_fps = qEnvironmentVariableIntValue("ASPIA_MIN_FPS", &ok);
    if (!ok)
        return kMinScreenCaptureFps;

    if (min_fps < 1 || min_fps > 60)
    {
        LOG(INFO) << "Environment variable contains an incorrect minimum FPS:" << min_fps;
        return kMinScreenCaptureFps;
    }

    LOG(INFO) << "Minimum FPS specified by environment variable";
    return min_fps;
}

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopAgent::DesktopAgent(QObject* parent)
    : QObject(parent),
      ipc_channel_(new base::IpcChannel(this)),
      desktop_environment_(base::DesktopEnvironment::create(this)),
      preferred_capturer_(static_cast<base::ScreenCapturer::Type>(SystemSettings().preferredVideoCapturer())),
      capture_timer_(new QTimer(this)),
      scale_reducer_(std::make_unique<base::ScaleReducer>(base::ScaleReducer::Quality::HIGH)),
      overflow_timer_(new QTimer(this)),
      default_fps_(defaultCaptureFps()),
      min_fps_(minCaptureFps()),
      max_fps_(maxCaptureFps())
{
    LOG(INFO) << "Ctor";

    connect(ipc_channel_, &base::IpcChannel::sig_connected, this, &DesktopAgent::onIpcConnected);
    connect(ipc_channel_, &base::IpcChannel::sig_disconnected, this, &DesktopAgent::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_errorOccurred, this, &DesktopAgent::onIpcErrorOccurred);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived, this, &DesktopAgent::onIpcMessageReceived);

    selectCapturer(base::ScreenCapturer::Error::SUCCEEDED);

    capture_scheduler_.setFps(defaultCaptureFps());
    capture_timer_->setTimerType(Qt::PreciseTimer);
    connect(capture_timer_, &QTimer::timeout, this, &DesktopAgent::onCaptureScreen);

    overflow_timer_->setInterval(std::chrono::milliseconds(1000));
    connect(overflow_timer_, &QTimer::timeout, this, &DesktopAgent::onOverflowCheck);

#if defined(Q_OS_WINDOWS)
    input_injector_ = new InputInjectorWin(this);

    // At the end of the user's session, the program ends later than the others.
    if (!SetProcessShutdownParameters(0, SHUTDOWN_NORETRY))
        PLOG(ERROR) << "SetProcessShutdownParameters failed";
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
    input_injector_ = InputInjectorX11::create();
#endif // defined(Q_OS_LINUX)
}

//--------------------------------------------------------------------------------------------------
DesktopAgent::~DesktopAgent()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::start(const QString& ipc_channel_name)
{
    ipc_channel_->connectTo(ipc_channel_name);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onIpcConnected()
{
    overflow_timer_->start();
    ipc_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onIpcDisconnected()
{
    LOG(ERROR) << "IPC channel is disconnected. Terminate application";
    base::Application::quit();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onIpcErrorOccurred()
{
    LOG(ERROR) << "Error when connection to IPC server. Terminate application";
    base::Application::quit();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onIpcMessageReceived(
    quint32 /* ipc_channel_id */, const QByteArray& buffer, bool /* reliable */)
{
    proto::desktop::ServiceToAgent message;
    if (!base::parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse message from service";
        return;
    }

    if (message.has_control())
    {
        const proto::desktop::AgentControl& control = message.control();
        const std::string& command_name = control.command_name();

        LOG(INFO) << "Command:" << command_name;

        if (command_name == "start_client")
        {
            CHECK(control.has_utf8_string());
            startClient(QString::fromStdString(control.utf8_string()));
        }
        else if (command_name == "pause")
        {
            CHECK(control.has_boolean());
            is_paused_ = control.boolean();
        }
        else if (command_name == "lock_mouse")
        {
            CHECK(control.has_boolean());
            is_mouse_locked_ = control.boolean();
        }
        else if (command_name == "lock_keyboard")
        {
            CHECK(control.has_boolean());
            is_keyboard_locked_ = control.boolean();
        }
        else if (command_name == "connection_changed")
        {
            // When changing the connection (for example, when switching from TCP to UDP), a keyframe,
            // resetting the cursor cache, etc. are required.
            onClientConfigured();
        }
        else
        {
            LOG(ERROR) << "Unhandled command:" << command_name;
        }
    }
    else
    {
        LOG(ERROR) << "Unhandled message from service";
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onClientConfigured()
{
    DesktopAgentClient::Config merged_config;

    for (auto* client : std::as_const(clients_))
    {
        const DesktopAgentClient::Config& config = client->config();

        if (config.video_encoding == proto::video::ENCODING_VP8)
            merged_config.video_encoding = proto::video::ENCODING_VP8;

        if (config.audio_encoding == proto::audio::ENCODING_OPUS)
            merged_config.audio_encoding = proto::audio::ENCODING_OPUS;

        // If at least one client has disabled effects, then the effects will be disabled for
        // everyone.
        merged_config.disable_effects = merged_config.disable_effects || config.disable_effects;

        // If at least one client has disabled the wallpaper, then the effects will be disabled for
        // everyone.
        merged_config.disable_wallpaper = merged_config.disable_wallpaper || config.disable_wallpaper;

        // If at least one client has enabled input block, then the block will be enabled for
        // everyone.
        merged_config.block_input = merged_config.block_input || config.block_input;

        merged_config.lock_at_disconnect = merged_config.lock_at_disconnect || config.lock_at_disconnect;
        merged_config.cursor_position = merged_config.cursor_position || config.cursor_position;
        merged_config.cursor_shape = merged_config.cursor_shape || config.cursor_shape;
    }

    LOG(INFO) << "Merged configuration (wallpaper:" << merged_config.disable_wallpaper
              << "effects:" << merged_config.disable_effects
              << "block_input:" << merged_config.block_input
              << "lock_at_disconnect:" << merged_config.lock_at_disconnect
              << "cursor_position:" << merged_config.cursor_position
              << "cursor_shape:" << merged_config.cursor_shape << ")";

    if (merged_config.video_encoding != proto::video::ENCODING_VP8 &&
        merged_config.video_encoding != proto::video::ENCODING_VP9)
    {
        LOG(ERROR) << "Unsupported video encoding:" << merged_config.video_encoding;
        return;
    }

    video_encoder_ = std::make_unique<base::VideoEncoder>(merged_config.video_encoding);

    switch (merged_config.audio_encoding)
    {
        case proto::audio::ENCODING_OPUS:
        {
            audio_encoder_ = std::make_unique<base::AudioEncoder>();

            if (!audio_capturer_)
            {
                audio_capturer_ = new base::AudioCapturerWrapper(this);
                connect(audio_capturer_, &base::AudioCapturerWrapper::sig_audioCaptured,
                        this, &DesktopAgent::encodeAudio, Qt::QueuedConnection);
                audio_capturer_->start();
            }
        }
        break;

        default:
        {
            if (audio_capturer_)
            {
                audio_capturer_->disconnect();
                audio_capturer_->deleteLater();
                audio_capturer_ = nullptr;
            }
            audio_encoder_.reset();
        }
        break;
    }

    cursor_encoder_.reset();
    if (merged_config.cursor_shape)
    {
        LOG(INFO) << "Cursor shape enabled. Init cursor encoder";
        cursor_encoder_ = std::make_unique<base::CursorEncoder>();
    }

    if (desktop_environment_)
    {
        desktop_environment_->setWallpaper(!merged_config.disable_wallpaper);
        desktop_environment_->setEffects(!merged_config.disable_effects);
    }

    if (input_injector_)
        input_injector_->setBlockInput(merged_config.block_input);

    is_cursor_position_ = merged_config.cursor_position;
    is_lock_at_disconnect_ = merged_config.lock_at_disconnect;

    capture_timer_->start(0);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onClientFinished()
{
    DesktopAgentClient* client = dynamic_cast<DesktopAgentClient*>(sender());
    CHECK(client);

    client->disconnect();
    client->deleteLater();
    clients_.removeOne(client);

    if (!clients_.isEmpty())
    {
        onClientConfigured();
        onPreferredSizeChanged();
        return;
    }

    LOG(INFO) << "Last desktop client disconnected";
    capture_timer_->stop();

    if (is_lock_at_disconnect_)
    {
        if (!base::PowerController::lock())
            LOG(ERROR) << "Unable to lock user session";
        else
            LOG(INFO) << "User session locked";
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onInjectMouseEvent(const proto::desktop::MouseEvent& event)
{
    if (is_paused_ || is_mouse_locked_ || !input_injector_)
        return;

    int pos_x = int(double(event.x() * 100) / scale_reducer_->scaleFactorX());
    int pos_y = int(double(event.y() * 100) / scale_reducer_->scaleFactorY());

    proto::desktop::MouseEvent out_event;
    out_event.set_mask(event.mask());
    out_event.set_x(pos_x);
    out_event.set_y(pos_y);

    input_injector_->injectMouseEvent(out_event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onInjectKeyEvent(const proto::desktop::KeyEvent& event)
{
    if (is_paused_ || is_keyboard_locked_ || !input_injector_)
        return;
    input_injector_->injectKeyEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onInjectTextEvent(const proto::desktop::TextEvent& event)
{
    if (is_paused_ || is_keyboard_locked_ || !input_injector_)
        return;
    input_injector_->injectTextEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onInjectTouchEvent(const proto::desktop::TouchEvent& event)
{
    if (is_paused_ || is_mouse_locked_ || is_keyboard_locked_ || !input_injector_)
        return;
    input_injector_->injectTouchEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onSelectScreen(const proto::desktop::Screen& screen)
{
    if (!screen_capturer_)
    {
        LOG(ERROR) << "Screen capturer not initialized";
        return;
    }

    base::ScreenCapturer::ScreenId screen_id = static_cast<base::ScreenCapturer::ScreenId>(screen.id());
    QSize resolution = base::parse(screen.resolution());

    selectScreen(screen_id, resolution);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onScreenListChanged(
    const base::ScreenCapturer::ScreenList& list, base::ScreenCapturer::ScreenId current)
{
    proto::desktop::ScreenList* screen_list = screen_message_.newMessage().mutable_screen_list();
    screen_list->set_current_screen(current);

    for (const auto& resolition_item : list.resolutions)
    {
        proto::desktop::Size* resolution = screen_list->add_resolution();
        resolution->set_width(resolition_item.width());
        resolution->set_height(resolition_item.height());
    }

    for (const auto& screen_item : list.screens)
    {
        proto::desktop::Screen* screen = screen_list->add_screen();
        screen->set_id(screen_item.id);
        screen->set_title(screen_item.title.toStdString());

        proto::desktop::Point* position = screen->mutable_position();
        position->set_x(screen_item.position.x());
        position->set_y(screen_item.position.y());

        proto::desktop::Size* resolution = screen->mutable_resolution();
        resolution->set_width(screen_item.resolution.width());
        resolution->set_height(screen_item.resolution.height());

        proto::desktop::Point* dpi = screen->mutable_dpi();
        dpi->set_x(screen_item.dpi.x());
        dpi->set_y(screen_item.dpi.y());

        if (screen_item.is_primary)
            screen_list->set_primary_screen(screen_item.id);
    }

    const QByteArray& buffer = screen_message_.serialize();
    for (auto* client : std::as_const(clients_))
        client->onScreenListData(buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onScreenTypeChanged(base::ScreenCapturer::ScreenType type, const QString& name)
{
    proto::desktop::ScreenType* screen_type = screen_message_.newMessage().mutable_screen_type();
    screen_type->set_name(name.toStdString());

    switch (type)
    {
        case base::ScreenCapturer::ScreenType::DESKTOP:
            screen_type->set_type(proto::desktop::ScreenType::TYPE_DESKTOP);
            break;
        case base::ScreenCapturer::ScreenType::LOCK:
            screen_type->set_type(proto::desktop::ScreenType::TYPE_LOCK);
            break;
        case base::ScreenCapturer::ScreenType::LOGIN:
            screen_type->set_type(proto::desktop::ScreenType::TYPE_LOGIN);
            break;
        case base::ScreenCapturer::ScreenType::OTHER:
            screen_type->set_type(proto::desktop::ScreenType::TYPE_OTHER);
            break;
        default:
            screen_type->set_type(proto::desktop::ScreenType::TYPE_UNKNOWN);
            break;
    }

    const QByteArray& buffer = screen_message_.serialize();
    for (auto* client : std::as_const(clients_))
        client->onScreenTypeData(buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onPreferredSizeChanged()
{
    QList<QSize> sizes;

    for (auto* client : std::as_const(clients_))
        sizes.emplace_back(client->preferredSize());

    QSize max_size = *std::max_element(sizes.begin(), sizes.end(), [](const QSize& a, const QSize& b)
    {
        return a.width() * a.height() < b.width() * b.height();
    });

    preferred_size_ = max_size;
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onKeyFrameRequested()
{
    if (video_encoder_)
        video_encoder_->setKeyFrameRequired(true);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onCaptureScreen()
{
    if (capture_scheduler_.isInProgress())
    {
        LOG(INFO) << "Capture in progress";
        return;
    }

    if (!screen_capturer_)
    {
        LOG(ERROR) << "Screen capturer is NOT initialized";
        return;
    }

    capture_scheduler_.onBeginCapture();

    if (is_paused_)
    {
        proto::video::Packet* video_packet = video_message_.newMessage().mutable_packet();
        video_packet->set_error_code(proto::video::ERROR_CODE_PAUSED);

        const QByteArray& buffer = video_message_.serialize();
        for (auto* client : std::as_const(clients_))
            client->onVideoData(buffer);

        capture_timer_->start(capture_scheduler_.nextCaptureDelay());
        return;
    }

    screen_capturer_->switchToInputDesktop();

    int count = screen_capturer_->screenCount();
    if (screen_count_ != count)
    {
        LOG(INFO) << "Screen count changed from" << count << "to" << screen_count_;

        screen_resizer_.reset();
        screen_resizer_ = base::DesktopResizer::create();

        screen_count_ = count;
        selectScreen(defaultScreen(), QSize());
    }

    base::ScreenCapturer::Error error;
    const base::Frame* frame = screen_capturer_->captureFrame(&error);
    if (!frame)
    {
        proto::video::ErrorCode error_code;

        switch (error)
        {
            case base::ScreenCapturer::Error::TEMPORARY:
                error_code = proto::video::ERROR_CODE_TEMPORARY;
                break;

            case base::ScreenCapturer::Error::PERMANENT:
            {
                error_code = proto::video::ERROR_CODE_PERMANENT;

                QTimer::singleShot(0, this, [this]()
                {
                    selectCapturer(base::ScreenCapturer::Error::PERMANENT);
                });
            }
            break;

            default:
                NOTREACHED();
                return;
        }

        proto::video::Packet* video_packet = video_message_.newMessage().mutable_packet();
        video_packet->set_error_code(error_code);

        const QByteArray& buffer = video_message_.serialize();
        for (auto* client : std::as_const(clients_))
            client->onVideoData(buffer);
    }
    else
    {
        if (input_injector_)
            input_injector_->setScreenOffset(frame->topLeft());

        encodeScreen(frame);
    }

    encodeCursor(screen_capturer_->captureCursor());

    if (is_cursor_position_)
    {
        QPoint cursor_pos = screen_capturer_->cursorPosition();

        int delta_x = std::abs(cursor_pos.x() - last_cursor_pos_.x());
        int delta_y = std::abs(cursor_pos.y() - last_cursor_pos_.y());

        if (delta_x > 1 || delta_y > 1)
        {
            int pos_x = int(double(cursor_pos.x()) * scale_reducer_->scaleFactorX() / 100.0);
            int pos_y = int(double(cursor_pos.y()) * scale_reducer_->scaleFactorY() / 100.0);

            proto::cursor::Position* position = cursor_message_.newMessage().mutable_position();
            position->set_x(pos_x);
            position->set_y(pos_y);

            const QByteArray& buffer = cursor_message_.serialize();
            for (auto* client : std::as_const(clients_))
                client->onCursorPositionData(buffer);
            last_cursor_pos_ = cursor_pos;
        }
    }

    capture_timer_->start(capture_scheduler_.nextCaptureDelay());
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onOverflowCheck()
{
    proto::desktop::Overflow::State state = proto::desktop::Overflow::STATE_NONE;
    qint64 minimal_bandwidth = std::numeric_limits<qint64>::max();

    for (auto* client : std::as_const(clients_))
    {
        if (client->overflowState() > state)
            state = client->overflowState();

        qint64 bandwidth = client->bandwidth();
        if (bandwidth != 0 && bandwidth < minimal_bandwidth)
            minimal_bandwidth = bandwidth;
    }

    qint64 bandwidth = 0;
    if (minimal_bandwidth != std::numeric_limits<qint64>::max())
        bandwidth = minimal_bandwidth;

    // When bandwidth is unknown, use a conservative limit.
    int bandwidth_fps_limit = 20;
    if (bandwidth > 0)
    {
        // Calculate FPS limit based on measured bandwidth.
        if (bandwidth < 100 * 1024)        // < 100 KB/s
            bandwidth_fps_limit = min_fps_;
        else if (bandwidth < 300 * 1024)   // < 300 KB/s
            bandwidth_fps_limit = 16;
        else if (bandwidth < 500 * 1024)   // < 500 KB/s
            bandwidth_fps_limit = 18;
        else if (bandwidth < 1024 * 1024)  // < 1 MB/s
            bandwidth_fps_limit = 20;
        else if (bandwidth < 2048 * 1024)  // < 2 MB/s
            bandwidth_fps_limit = 24;
        else
            bandwidth_fps_limit = max_fps_;
    }

    int effective_max_fps = std::min(max_fps_, bandwidth_fps_limit);

    int current_fps = capture_scheduler_.fps();
    int next_fps = current_fps;

    // If the current FPS exceeds the bandwidth limit, reduce immediately.
    if (current_fps > effective_max_fps)
    {
        next_fps = effective_max_fps;
    }
    else if (state == proto::desktop::Overflow::STATE_CRITICAL)
    {
        pressure_score_ = std::min(100, pressure_score_ + 20);
        stable_seconds_ = 0;
        cooldown_seconds_ = 15;

        next_fps = std::max(min_fps_, std::min(current_fps - 3, default_fps_));
    }
    else if (state == proto::desktop::Overflow::STATE_WARNING)
    {
        pressure_score_ = std::min(100, pressure_score_ + 8);
        stable_seconds_ = 0;

        next_fps = std::max(min_fps_, std::min(current_fps - 2, default_fps_));
    }
    else
    {
        pressure_score_ = std::max(0, pressure_score_ - 3);
        ++stable_seconds_;
        if (cooldown_seconds_ > 0)
            --cooldown_seconds_;

        if (stable_seconds_ >= 15 && cooldown_seconds_ == 0)
        {
            int max_fps = effective_max_fps;

            if (pressure_score_ >= 80)
                max_fps = min_fps_;
            else if (pressure_score_ >= 60)
                max_fps = 18;
            else if (pressure_score_ >= 40)
                max_fps = 20;
            else if (pressure_score_ >= 20)
                max_fps = 22;

            if (current_fps < std::min(max_fps, effective_max_fps))
                next_fps = current_fps + 1;
        }
    }

    if (current_fps != next_fps)
        capture_scheduler_.setFps(next_fps);

    auto scaled_size = [](const QSize& size, double factor) -> QSize
    {
        return QSize(double(size.width()) * factor, double(size.height()) * factor);
    };

    QSize forced_size = forced_size_;

    if (bandwidth > 0 && bandwidth < 30 * 1024) // < 30 KB/s
        forced_size = scaled_size(source_size_, 0.6);
    else if (bandwidth > 0 && bandwidth < 100 * 1024) // < 100 KB/s
        forced_size = scaled_size(source_size_, 0.7);
    else if (pressure_score_ >= 90)
        forced_size = scaled_size(source_size_, 0.7);
    else if (pressure_score_ >= 80)
        forced_size = scaled_size(source_size_, 0.8);
    else if (pressure_score_ >= 70)
        forced_size = scaled_size(source_size_, 0.9);
    else
        forced_size = QSize();

    if (forced_size != forced_size_)
    {
        LOG(INFO) << "Forced size changed from" << forced_size_ << "to" << forced_size;
        forced_size_ = forced_size;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::startClient(const QString& ipc_channel_name)
{
    CHECK(ipc_channel_);

    DesktopAgentClient* client = new DesktopAgentClient(this);
    clients_.append(client);

    connect(client, &DesktopAgentClient::sig_injectMouseEvent, this, &DesktopAgent::onInjectMouseEvent);
    connect(client, &DesktopAgentClient::sig_injectKeyEvent, this, &DesktopAgent::onInjectKeyEvent);
    connect(client, &DesktopAgentClient::sig_injectTextEvent, this, &DesktopAgent::onInjectTextEvent);
    connect(client, &DesktopAgentClient::sig_injectTouchEvent, this, &DesktopAgent::onInjectTouchEvent);
    connect(client, &DesktopAgentClient::sig_selectScreen, this, &DesktopAgent::onSelectScreen);
    connect(client, &DesktopAgentClient::sig_preferredSizeChanged, this, &DesktopAgent::onPreferredSizeChanged);
    connect(client, &DesktopAgentClient::sig_keyFrameRequested, this, &DesktopAgent::onKeyFrameRequested);
    connect(client, &DesktopAgentClient::sig_configured, this, &DesktopAgent::onClientConfigured);
    connect(client, &DesktopAgentClient::sig_finished, this, &DesktopAgent::onClientFinished);

    LOG(INFO) << "Starting client...";
    client->start(ipc_channel_name);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::selectCapturer(base::ScreenCapturer::Error last_error)
{
    LOG(INFO) << "Selecting screen capturer. Preferred capturer:" << preferred_capturer_;

    if (screen_capturer_)
    {
        screen_capturer_->disconnect();
        screen_capturer_->deleteLater();
        screen_capturer_ = nullptr;
    }

#if defined(Q_OS_WINDOWS)
    screen_capturer_ = base::ScreenCapturerWin::create(preferred_capturer_, last_error, this);
#elif defined(Q_OS_LINUX)
    screen_capturer_ = base::ScreenCapturerX11::create(this);
    if (!screen_capturer_)
    {
        LOG(ERROR) << "Unable to create X11 screen capturer";
        return;
    }
#else
    NOTIMPLEMENTED();
#endif

    if (!screen_capturer_)
    {
        LOG(ERROR) << "Unable to create screen capturer";
        return;
    }

    LOG(INFO) << "Selected screen capturer:" << screen_capturer_->type();

    connect(screen_capturer_, &base::ScreenCapturer::sig_screenTypeChanged,
            this, &DesktopAgent::onScreenTypeChanged);

    connect(screen_capturer_, &base::ScreenCapturer::sig_desktopChanged, this, [this]()
    {
        if (!desktop_environment_)
        {
            LOG(ERROR) << "Desktop environment is not initialized";
            return;
        }

        desktop_environment_->onDesktopChanged();
    });

    if (last_screen_id_ != base::ScreenCapturer::kInvalidScreenId)
    {
        LOG(INFO) << "Restore selected screen:" << last_screen_id_;
        selectScreen(last_screen_id_, QSize());
    }
}

//--------------------------------------------------------------------------------------------------
base::ScreenCapturer::ScreenId DesktopAgent::defaultScreen()
{
    if (!screen_capturer_)
    {
        LOG(ERROR) << "Screen capturer is not initialized";
        return base::ScreenCapturer::kInvalidScreenId;
    }

    base::ScreenCapturer::ScreenList screen_list;
    if (!screen_capturer_->screenList(&screen_list))
    {
        LOG(ERROR) << "ScreenCapturer::screenList failed";
        return base::ScreenCapturer::kFullDesktopScreenId;
    }

    for (const auto& screen : std::as_const(screen_list.screens))
    {
        if (screen.is_primary)
        {
            LOG(INFO) << "Primary screen found:" << screen.id;
            return screen.id;
        }
    }

    LOG(INFO) << "Primary screen NOT found";
    return base::ScreenCapturer::kFullDesktopScreenId;
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::selectScreen(base::ScreenCapturer::ScreenId screen_id, const QSize& resolution)
{
    if (screen_id == screen_capturer_->currentScreen() && !resolution.isEmpty() && screen_resizer_)
    {
        LOG(INFO) << "Change resolution for screen" << screen_id << "to:" << resolution;
        if (!screen_resizer_->setResolution(screen_id, resolution))
        {
            LOG(ERROR) << "setResolution failed";
            return;
        }
    }
    else
    {
        LOG(INFO) << "Select screen:" << screen_id;
        if (!screen_capturer_->selectScreen(screen_id))
        {
            LOG(ERROR) << "ScreenCapturer::selectScreen failed";
            return;
        }

        last_screen_id_ = screen_id;
    }

    base::ScreenCapturer::ScreenList screen_list;
    if (!screen_capturer_->screenList(&screen_list))
    {
        LOG(ERROR) << "ScreenCapturer::screenList failed";
        return;
    }

    if (screen_resizer_)
    {
        screen_list.resolutions = screen_resizer_->supportedResolutions(screen_id);
        if (screen_list.resolutions.isEmpty())
            LOG(INFO) << "No supported resolutions";

        for (const auto& resolition : std::as_const(screen_list.resolutions))
            LOG(INFO) << "Supported resolution:" << resolition;
    }

    for (const auto& screen : std::as_const(screen_list.screens))
    {
        LOG(INFO) << "Screen #" << screen.id << "(position:" << screen.position
                  << "resolution:" << screen.resolution << "DPI:" << screen.dpi << ")";
    }

    onScreenListChanged(screen_list, screen_id);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::encodeScreen(const base::Frame* frame)
{
    if (!frame || !video_encoder_)
        return;

    if (frame->constUpdatedRegion().isEmpty() && frame_count_ > 0)
        return;

    ++frame_count_;

    if (source_size_ != frame->size())
    {
        // Every time we change the resolution, we have to reset the preferred size.
        source_size_ = frame->size();
        preferred_size_ = QSize(0, 0);
        forced_size_ = QSize(0, 0);
    }

    QSize current_size = preferred_size_;

    // If the preferred size is larger than the original, then we use the original size.
    if (current_size.width() > source_size_.width() || current_size.height() > source_size_.height())
        current_size = source_size_;

    // If we don't have a preferred size, then we use the original frame size.
    if (current_size.isEmpty())
        current_size = source_size_;

    if (!forced_size_.isEmpty())
    {
        int forced = forced_size_.width() * forced_size_.height();
        int current = current_size.width() * current_size.height();

        if (forced < current)
            current_size = forced_size_;
    }

    const base::Frame* scaled_frame = scale_reducer_->scaleFrame(frame, current_size);
    if (!scaled_frame)
    {
        LOG(ERROR) << "No scaled frame";
        return;
    }

    proto::video::Data& message = video_message_.newMessage();
    proto::video::Packet* packet = message.mutable_packet();

    // Encode the frame into a video packet.
    if (!video_encoder_->encode(scaled_frame, packet))
    {
        LOG(ERROR) << "Unable to encode video packet";
        return;
    }

    if (packet->has_format())
    {
        proto::video::PacketFormat* format = packet->mutable_format();

        // In video packets that contain the format, we pass the screen capture type.
        format->set_capturer_type(frame->capturerType());

        // Real screen size.
        proto::video::Size* screen_size = format->mutable_screen_size();
        screen_size->set_width(frame->size().width());
        screen_size->set_height(frame->size().height());

        LOG(INFO) << "Video packet has format:" << *format;
    }

    const QByteArray& buffer = video_message_.serialize();
    for (auto* client : std::as_const(clients_))
        client->onVideoData(buffer);

    video_encoder_->setEncodeBuffer(std::move(*message.mutable_packet()->mutable_data()));
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::encodeCursor(const base::MouseCursor* cursor)
{
    if (!cursor || !cursor_encoder_)
        return;

    proto::cursor::Data& message = cursor_message_.newMessage();
    if (!cursor_encoder_->encode(*cursor, message.mutable_shape()))
        return;

    const QByteArray& buffer = cursor_message_.serialize();
    for (auto* client : std::as_const(clients_))
        client->onCursorData(buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::encodeAudio(const proto::audio::Packet& packet)
{
    if (!audio_encoder_)
        return;

    if (!audio_encoder_->encode(packet, audio_message_.newMessage().mutable_packet()))
        return;

    const QByteArray& buffer = audio_message_.serialize();
    for (auto* client : std::as_const(clients_))
        client->onAudioData(buffer);
}

} // namespace host
