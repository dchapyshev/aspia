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
#include "base/desktop/screen_capturer_wrapper.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "host/desktop_agent_client.h"
#include "host/system_settings.h"
#include "common/clipboard_monitor.h"
#include "proto/desktop_internal.h"

#if defined(Q_OS_WINDOWS)
#include "base/desktop/desktop_environment_win.h"
#include "host/input_injector_win.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include "host/input_injector_x11.h"
#endif // defined(Q_OS_LINUX)

namespace host {

//--------------------------------------------------------------------------------------------------
DesktopAgent::DesktopAgent(QObject* parent)
    : QObject(parent),
      ipc_channel_(new base::IpcChannel(this)),
      clipboard_(new common::ClipboardMonitor(this)),
      screen_capturer_(new base::ScreenCapturerWrapper(
          static_cast<base::ScreenCapturer::Type>(SystemSettings().preferredVideoCapturer()), this)),
      audio_capturer_(new base::AudioCapturerWrapper(this)),
      screen_capture_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    connect(ipc_channel_, &base::IpcChannel::sig_connected, this, &DesktopAgent::onIpcConnected);
    connect(ipc_channel_, &base::IpcChannel::sig_disconnected, this, &DesktopAgent::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_errorOccurred, this, &DesktopAgent::onIpcErrorOccurred);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived, this, &DesktopAgent::onIpcMessageReceived);

    capture_scheduler_.setUpdateInterval(std::chrono::milliseconds(40));
    screen_capture_timer_->setTimerType(Qt::PreciseTimer);

    connect(screen_capture_timer_, &QTimer::timeout, this, &DesktopAgent::onCaptureScreen);

#if defined(Q_OS_WINDOWS)
    input_injector_ = new InputInjectorWin(this);
#elif defined(Q_OS_LINUX)
    input_injector_ = InputInjectorX11::create();
#else
    LOG(ERROR) << "Input injector not supported for platform";
#endif

#if defined(Q_OS_WINDOWS)
    // At the end of the user's session, the program ends later than the others.
    if (!SetProcessShutdownParameters(0, SHUTDOWN_NORETRY))
        PLOG(ERROR) << "SetProcessShutdownParameters failed";

    connect(base::Application::instance(), &base::Application::sig_queryEndSession, []()
    {
        base::DesktopEnvironmentWin::updateEnvironment();
    });
#endif // defined(Q_OS_WINDOWS)
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

    screen_capturer_->enableWallpaper(!merged_config.disable_wallpaper);
    screen_capturer_->enableEffects(!merged_config.disable_effects);
    screen_capturer_->enableFontSmoothing(!merged_config.disable_font_smoothing);
    screen_capturer_->enableCursorPosition(merged_config.cursor_position);

    if (input_injector_)
        input_injector_->setBlockInput(merged_config.block_input);

    lock_at_disconnect_ = merged_config.lock_at_disconnect;
    clear_clipboard_ = merged_config.clear_clipboard;

    onCaptureScreen();
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

    client->disconnect(); // Disoconnect all signals.
    client->deleteLater();
    clients_.removeOne(client);

    if (!clients_.isEmpty())
        return;

    LOG(INFO) << "Last desktop client disconnected";
    screen_capture_timer_->stop();

    if (clear_clipboard_)
    {
        LOG(INFO) << "Clearing clipboard";
        clipboard_->clearClipboard();
    }

    if (lock_at_disconnect_)
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
void DesktopAgent::onCaptureScreen()
{
    capture_scheduler_.onBeginCapture();

    if (is_paused_)
    {
        for (const auto& client : std::as_const(clients_))
            client->onScreenCaptureError(proto::desktop::VIDEO_ERROR_CODE_PAUSED);

        capture_scheduler_.onEndCapture();
        screen_capture_timer_->start(capture_scheduler_.nextCaptureDelay());
        return;
    }

    const base::Frame* frame = nullptr;
    const base::MouseCursor* cursor = nullptr;

    base::ScreenCapturer::Error error = screen_capturer_->captureFrame(&frame, &cursor);
    if (error != base::ScreenCapturer::Error::SUCCEEDED)
    {
        proto::desktop::VideoErrorCode error_code;

        switch (error)
        {
            case base::ScreenCapturer::Error::PERMANENT:
                error_code = proto::desktop::VIDEO_ERROR_CODE_PERMANENT;
                break;

            case base::ScreenCapturer::Error::TEMPORARY:
                error_code = proto::desktop::VIDEO_ERROR_CODE_TEMPORARY;
                break;

            default:
                NOTREACHED();
                return;
        }

        for (const auto& client : std::as_const(clients_))
            client->onScreenCaptureError(error_code);
    }
    else
    {
        if (input_injector_)
            input_injector_->setScreenOffset(frame->topLeft());

        for (const auto& client : std::as_const(clients_))
            client->onScreenCaptureData(frame, cursor);
    }

    capture_scheduler_.onEndCapture();
    screen_capture_timer_->start(capture_scheduler_.nextCaptureDelay());
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

    connect(screen_capturer_, &base::ScreenCapturerWrapper::sig_cursorPositionChanged,
            client, &DesktopAgentClient::onCursorPositionChanged);
    connect(client, &DesktopAgentClient::sig_selectScreen,
            screen_capturer_, &base::ScreenCapturerWrapper::selectScreen);
    connect(screen_capturer_, &base::ScreenCapturerWrapper::sig_screenTypeChanged,
            client, &DesktopAgentClient::onScreenTypeChanged);
    connect(screen_capturer_, &base::ScreenCapturerWrapper::sig_screenListChanged,
            client, &DesktopAgentClient::onScreenListChanged);

    connect(audio_capturer_, &base::AudioCapturerWrapper::sig_audioCaptured,
            client, &DesktopAgentClient::onAudioCaptureData, Qt::QueuedConnection);

    connect(client, &DesktopAgentClient::sig_captureScreen, this, &DesktopAgent::onCaptureScreen);
    connect(client, &DesktopAgentClient::sig_configured, this, &DesktopAgent::onClientConfigured);
    connect(client, &DesktopAgentClient::sig_finished, this, &DesktopAgent::onClientFinished);

    LOG(INFO) << "Starting client...";
    client->start(ipc_channel_name);
}

} // namespace host
