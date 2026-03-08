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
#include "base/desktop/desktop_resizer.h"
#include "base/desktop/desktop_environment.h"
#include "base/desktop/mouse_cursor.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "host/desktop_agent_client.h"
#include "host/system_settings.h"
#include "common/clipboard_monitor.h"
#include "proto/desktop_internal.h"

#if defined(Q_OS_WINDOWS)
#include "base/desktop/desktop_environment_win.h"
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
constexpr int kMinScreenCaptureFps = 14;
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
      clipboard_(new common::ClipboardMonitor(this)),
      audio_capturer_(new base::AudioCapturerWrapper(this)),
      desktop_environment_(base::DesktopEnvironment::create(this)),
      preferred_capturer_(static_cast<base::ScreenCapturer::Type>(SystemSettings().preferredVideoCapturer())),
      capture_timer_(new QTimer(this)),
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

    connect(base::Application::instance(), &base::Application::sig_queryEndSession, []()
    {
        base::DesktopEnvironmentWin::updateEnvironment();
    });
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
    clipboard_->start();
    audio_capturer_->start();
    ipc_channel_->resume();
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
void DesktopAgent::onIpcMessageReceived(quint32 /* ipc_channel_id */, const QByteArray& buffer)
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

    for (const auto& client : std::as_const(clients_))
    {
        const DesktopAgentClient::Config& config = client->config();

        // If at least one client has disabled font smoothing, then the font smoothing will be
        // disabled for everyone.
        merged_config.disable_font_smoothing =
            merged_config.disable_font_smoothing || config.disable_font_smoothing;

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
        merged_config.clear_clipboard = merged_config.clear_clipboard || config.clear_clipboard;
        merged_config.cursor_position = merged_config.cursor_position || config.cursor_position;
    }

    LOG(INFO) << "Merged configuration (wallpaper:" << merged_config.disable_wallpaper
              << "effects:" << merged_config.disable_effects
              << "font_smoothing:" << merged_config.disable_font_smoothing
              << "block_input:" << merged_config.block_input
              << "lock_at_disconnect:" << merged_config.lock_at_disconnect
              << "clear_clipboard:" << merged_config.clear_clipboard
              << "cursor_position:" << merged_config.cursor_position;

    if (desktop_environment_)
    {
        desktop_environment_->setWallpaper(!merged_config.disable_wallpaper);
        desktop_environment_->setEffects(!merged_config.disable_effects);
        desktop_environment_->setFontSmoothing(!merged_config.disable_font_smoothing);
    }

    if (input_injector_)
        input_injector_->setBlockInput(merged_config.block_input);

    is_cursor_position_ = merged_config.cursor_position;
    is_lock_at_disconnect_ = merged_config.lock_at_disconnect;
    is_clear_clipboard_ = merged_config.clear_clipboard;

    capture_timer_->start(0);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onClientFinished()
{
    DesktopAgentClient* client = dynamic_cast<DesktopAgentClient*>(sender());
    if (!client)
    {
        LOG(ERROR) << "Unknown sender for finish slot";
        return;
    }

    client->disconnect();
    client->deleteLater();
    clients_.removeOne(client);

    if (!clients_.isEmpty())
        return;

    LOG(INFO) << "Last desktop client disconnected";
    capture_timer_->stop();

    if (is_clear_clipboard_)
    {
        LOG(INFO) << "Clearing clipboard";
        clipboard_->clearClipboard();
    }

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
    input_injector_->injectMouseEvent(event);
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
void DesktopAgent::onSelectScreen(base::ScreenCapturer::ScreenId screen_id, const QSize& resolution)
{
    if (!screen_capturer_)
    {
        LOG(ERROR) << "Screen capturer not initialized";
        return;
    }

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
        if (screen_list.resolutions.empty())
            LOG(INFO) << "No supported resolutions";

        for (const auto& resolition : std::as_const(screen_list.resolutions))
            LOG(INFO) << "Supported resolution:" << resolition;
    }

    for (const auto& screen : std::as_const(screen_list.screens))
    {
        LOG(INFO) << "Screen #" << screen.id << "(position:" << screen.position
                  << "resolution:" << screen.resolution << "DPI:" << screen.dpi << ")";
    }

    emit sig_screenListChanged(screen_list, screen_id);
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
        for (const auto& client : std::as_const(clients_))
            client->onScreenCaptureError(proto::desktop::VIDEO_ERROR_CODE_PAUSED);

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
        onSelectScreen(defaultScreen(), QSize());
    }

    base::ScreenCapturer::Error error;
    const base::Frame* frame = screen_capturer_->captureFrame(&error);
    if (!frame)
    {
        proto::desktop::VideoErrorCode error_code;

        switch (error)
        {
            case base::ScreenCapturer::Error::TEMPORARY:
                error_code = proto::desktop::VIDEO_ERROR_CODE_TEMPORARY;
                break;

            case base::ScreenCapturer::Error::PERMANENT:
            {
                error_code = proto::desktop::VIDEO_ERROR_CODE_PERMANENT;

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

        for (auto* client : std::as_const(clients_))
            client->onScreenCaptureError(error_code);
    }
    else
    {
        if (input_injector_)
            input_injector_->setScreenOffset(frame->topLeft());

        for (auto* client : std::as_const(clients_))
            client->onScreenCaptureData(frame);
    }

    const base::MouseCursor* cursor = screen_capturer_->captureCursor();
    if (cursor)
    {
        for (auto* client : std::as_const(clients_))
            client->onCursorCaptureData(cursor);
    }

    if (is_cursor_position_)
    {
        QPoint cursor_pos = screen_capturer_->cursorPosition();

        int delta_x = std::abs(cursor_pos.x() - last_cursor_pos_.x());
        int delta_y = std::abs(cursor_pos.y() - last_cursor_pos_.y());

        if (delta_x > 1 || delta_y > 1)
        {
            for (auto* client : std::as_const(clients_))
                client->onCursorPositionChanged(cursor_pos);
            last_cursor_pos_ = cursor_pos;
        }
    }

    capture_timer_->start(capture_scheduler_.nextCaptureDelay());
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onOverflowCheck()
{
    proto::desktop::Overflow::State state = proto::desktop::Overflow::STATE_NONE;

    for (auto* client : std::as_const(clients_))
    {
        if (client->overflowState() > state)
            state = client->overflowState();
    }

    int current_fps = capture_scheduler_.fps();
    int next_fps = current_fps;

    if (state == proto::desktop::Overflow::STATE_CRITICAL)
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
            int max_fps = max_fps_;

            if (pressure_score_ >= 80)
                max_fps = min_fps_;
            else if (pressure_score_ >= 60)
                max_fps = 18;
            else if (pressure_score_ >= 40)
                max_fps = 20;
            else if (pressure_score_ >= 20)
                max_fps = 22;

            if (current_fps < std::min(max_fps, max_fps_))
                next_fps = current_fps + 1;
        }
    }

    if (current_fps != next_fps)
        capture_scheduler_.setFps(next_fps);
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

    connect(client, &DesktopAgentClient::sig_injectClipboardEvent,
            clipboard_, &common::ClipboardMonitor::injectClipboardEvent);
    connect(clipboard_, &common::ClipboardMonitor::sig_clipboardEvent,
            client, &DesktopAgentClient::onClipboardEvent);

    connect(this, &DesktopAgent::sig_screenTypeChanged, client, &DesktopAgentClient::onScreenTypeChanged);
    connect(this, &DesktopAgent::sig_screenListChanged, client, &DesktopAgentClient::onScreenListChanged);
    connect(client, &DesktopAgentClient::sig_selectScreen, this, &DesktopAgent::onSelectScreen);

    connect(audio_capturer_, &base::AudioCapturerWrapper::sig_audioCaptured,
            client, &DesktopAgentClient::onAudioCaptureData, Qt::QueuedConnection);

    // We can connect via a queue here to prevent possible screen capture from starting while the
    // screen capture is in progress.
    connect(client, &DesktopAgentClient::sig_captureScreen,
            this, &DesktopAgent::onCaptureScreen, Qt::QueuedConnection);

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
            this, &DesktopAgent::sig_screenTypeChanged);

    connect(screen_capturer_, &base::ScreenCapturer::sig_desktopChanged, this, [this]()
    {
        if (!desktop_environment_)
        {
            LOG(ERROR) << "Desktop environment not initialized";
            return;
        }

        desktop_environment_->onDesktopChanged();
    });

    if (last_screen_id_ != base::ScreenCapturer::kInvalidScreenId)
    {
        LOG(INFO) << "Restore selected screen:" << last_screen_id_;
        onSelectScreen(last_screen_id_, QSize());
    }
}

//--------------------------------------------------------------------------------------------------
base::ScreenCapturer::ScreenId DesktopAgent::defaultScreen()
{
    if (!screen_capturer_)
    {
        LOG(ERROR) << "Screen capturer not initialized";
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

} // namespace host
