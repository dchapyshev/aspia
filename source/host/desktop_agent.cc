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
      ipc_server_(new base::IpcServer(this)),
      clipboard_monitor_(new common::ClipboardMonitor(this)),
      screen_capturer_(new base::ScreenCapturerWrapper(
          static_cast<base::ScreenCapturer::Type>(SystemSettings().preferredVideoCapturer()), this)),
      audio_capturer_(new base::AudioCapturerWrapper(this)),
      screen_capture_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    connect(ipc_server_, &base::IpcServer::sig_newConnection, this, &DesktopAgent::onIpcNewConnection);
    connect(ipc_server_, &base::IpcServer::sig_errorOccurred, this, &DesktopAgent::onIpcErrorOccurred);

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
    {
        PLOG(ERROR) << "SetProcessShutdownParameters failed";
    }

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
bool DesktopAgent::start(const QString& ipc_channel_name)
{
    if (!ipc_server_->start(ipc_channel_name))
    {
        LOG(ERROR) << "Unable to start IPC server";
        return false;
    }

    clipboard_monitor_->start();
    audio_capturer_->start();
    return true;
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onIpcNewConnection()
{
    while (ipc_server_->hasPendingConnections())
    {
        base::IpcChannel* ipc_channel = ipc_server_->nextPendingConnection();
        if (!ipc_channel)
        {
            LOG(ERROR) << "Invalid IPC channel pointer";
            continue;
        }

        DesktopAgentClient* client = new DesktopAgentClient(ipc_channel, this);

        connect(client, &DesktopAgentClient::sig_injectMouseEvent,
                input_injector_, &InputInjector::injectMouseEvent);
        connect(client, &DesktopAgentClient::sig_injectKeyEvent,
                input_injector_, &InputInjector::injectKeyEvent);
        connect(client, &DesktopAgentClient::sig_injectTextEvent,
                input_injector_, &InputInjector::injectTextEvent);
        connect(client, &DesktopAgentClient::sig_injectTouchEvent,
                input_injector_, &InputInjector::injectTouchEvent);

        connect(client, &DesktopAgentClient::sig_injectClipboardEvent,
                clipboard_monitor_, &common::ClipboardMonitor::injectClipboardEvent);
        connect(clipboard_monitor_, &common::ClipboardMonitor::sig_clipboardEvent,
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

        connect(client, &DesktopAgentClient::sig_captureScreen,
                this, &DesktopAgent::onCaptureScreen);
        connect(client, &DesktopAgentClient::sig_configured,
                this, &DesktopAgent::onClientConfigured);
        connect(client, &DesktopAgentClient::sig_finished,
                this, &DesktopAgent::onClientFinished);

        clients_.append(client);

        LOG(INFO) << "New IPC connection. Starting client...";
        client->start();
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onIpcErrorOccurred()
{
    LOG(ERROR) << "Error in IPC server. Terminate application";
    base::Application::quit();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onClientConfigured()
{
    DesktopAgentClient::Config system_config;
    memset(&system_config, 0, sizeof(system_config));

    for (const auto& client : std::as_const(clients_))
    {
        const DesktopAgentClient::Config& client_config = client->config();

        // If at least one client has disabled font smoothing, then the font smoothing will be
        // disabled for everyone.
        system_config.disable_font_smoothing =
            system_config.disable_font_smoothing || client_config.disable_font_smoothing;

        // If at least one client has disabled effects, then the effects will be disabled for
        // everyone.
        system_config.disable_effects =
            system_config.disable_effects || client_config.disable_effects;

        // If at least one client has disabled the wallpaper, then the effects will be disabled for
        // everyone.
        system_config.disable_wallpaper =
            system_config.disable_wallpaper || client_config.disable_wallpaper;

        // If at least one client has enabled input block, then the block will be enabled for
        // everyone.
        system_config.block_input = system_config.block_input || client_config.block_input;

        system_config.lock_at_disconnect =
            system_config.lock_at_disconnect || client_config.lock_at_disconnect;

        system_config.clear_clipboard =
            system_config.clear_clipboard || client_config.clear_clipboard;

        system_config.cursor_position =
            system_config.cursor_position || client_config.cursor_position;
    }

    LOG(INFO) << "Merged configuration";
    LOG(INFO) << "Disable wallpaper:" << system_config.disable_wallpaper;
    LOG(INFO) << "Disable effects:" << system_config.disable_effects;
    LOG(INFO) << "Disable font smoothing:" << system_config.disable_font_smoothing;
    LOG(INFO) << "Block input:" << system_config.block_input;
    LOG(INFO) << "Lock at disconnect:" << system_config.lock_at_disconnect;
    LOG(INFO) << "Clear clipboard:" << system_config.clear_clipboard;
    LOG(INFO) << "Cursor position:" << system_config.cursor_position;

    screen_capturer_->enableWallpaper(!system_config.disable_wallpaper);
    screen_capturer_->enableEffects(!system_config.disable_effects);
    screen_capturer_->enableFontSmoothing(!system_config.disable_font_smoothing);
    screen_capturer_->enableCursorPosition(system_config.cursor_position);

    if (input_injector_)
        input_injector_->setBlockInput(system_config.block_input);

    lock_at_disconnect_ = system_config.lock_at_disconnect;
    clear_clipboard_ = system_config.clear_clipboard;

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

    LOG(INFO) << "Last desktop client disconnected. Terminate application";
    screen_capture_timer_->stop();

    if (clear_clipboard_)
    {
        LOG(INFO) << "Clearing clipboard";
        clipboard_monitor_->clearClipboard();
    }

    if (lock_at_disconnect_)
    {
        if (!base::PowerController::lock())
            LOG(ERROR) << "Unable to lock user session";
        else
            LOG(INFO) << "User session locked";
    }

    base::Application::quit();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onCaptureScreen()
{
    capture_scheduler_.onBeginCapture();

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
        return;
    }

    if (frame || cursor)
    {
        if (frame && input_injector_)
            input_injector_->setScreenOffset(frame->topLeft());

        for (const auto& client : std::as_const(clients_))
            client->onScreenCaptureData(frame, cursor);
    }

    capture_scheduler_.onEndCapture();
    screen_capture_timer_->start(capture_scheduler_.nextCaptureDelay());
}

} // namespace host
