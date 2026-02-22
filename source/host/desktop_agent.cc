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

#include "base/logging.h"
#include "base/power_controller.h"
#include "base/ipc/ipc_channel.h"

#if defined(Q_OS_WINDOWS)
#include "host/input_injector_win.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include "host/input_injector_x11.h"
#endif // defined(Q_OS_LINUX)

namespace host {

//--------------------------------------------------------------------------------------------------
DesktopAgent::DesktopAgent(QObject* parent)
    : QObject(parent),
      screen_capture_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    screen_capture_timer_->setTimerType(Qt::PreciseTimer);
    connect(screen_capture_timer_, &QTimer::timeout, this, &DesktopAgent::onCaptureScreen);
}

//--------------------------------------------------------------------------------------------------
DesktopAgent::~DesktopAgent()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool DesktopAgent::start(const QString& ipc_channel_name)
{
    if (ipc_server_)
    {
        LOG(ERROR) << "Desktop agent already started";
        return false;
    }

    ipc_server_ = new base::IpcServer(this);

    connect(ipc_server_, &base::IpcServer::sig_newConnection, this, &DesktopAgent::onIpcNewConnection);
    connect(ipc_server_, &base::IpcServer::sig_errorOccurred, this, &DesktopAgent::onIpcErrorOccurred);

    if (!ipc_server_->start(ipc_channel_name))
    {
        LOG(ERROR) << "Unable to start IPC server";
        return false;
    }

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

        connect(client, &DesktopAgentClient::sig_injectMouseEvent, this, &DesktopAgent::onInjectMouseEvent);
        connect(client, &DesktopAgentClient::sig_injectKeyEvent, this, &DesktopAgent::onInjectKeyEvent);
        connect(client, &DesktopAgentClient::sig_injectTextEvent, this, &DesktopAgent::onInjectTextEvent);
        connect(client, &DesktopAgentClient::sig_injectTouchEvent, this, &DesktopAgent::onInjectTouchEvent);
        connect(client, &DesktopAgentClient::sig_captureScreen, this, &DesktopAgent::onCaptureScreen);
        connect(client, &DesktopAgentClient::sig_configured, this, &DesktopAgent::onClientConfigured);
        connect(client, &DesktopAgentClient::sig_finished, this, &DesktopAgent::onClientFinished);

        clients_.append(client);

        LOG(INFO) << "New IPC connection. Starting client...";
        client->start();
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onIpcErrorOccurred()
{
    LOG(ERROR) << "Error in IPC server. Terminate application";
    QCoreApplication::quit();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onClientConfigured()
{
    if (!input_injector_)
    {
#if defined(Q_OS_WINDOWS)
        input_injector_ = new InputInjectorWin(this);
#elif defined(Q_OS_LINUX)
        input_injector_ = InputInjectorX11::create();
#else
        LOG(ERROR) << "Input injector not supported for platform";
#endif
    }

    if (!clipboard_monitor_)
    {
        // A window is created to monitor the clipboard. We cannot create windows in the current
        // thread. Create a separate thread.
        clipboard_monitor_ = new common::ClipboardMonitor(this);
        connect(clipboard_monitor_, &common::ClipboardMonitor::sig_clipboardEvent,
                this, &DesktopAgent::onClipboardEvent);
        clipboard_monitor_->start();
    }

    if (!screen_capturer_)
    {
        capture_scheduler_.setUpdateInterval(std::chrono::milliseconds(40));

        screen_capturer_ = new base::ScreenCapturerWrapper(preferred_video_capturer_, this);

        connect(screen_capturer_, &base::ScreenCapturerWrapper::sig_screenListChanged,
                this, &DesktopAgent::onScreenListChanged);
        connect(screen_capturer_, &base::ScreenCapturerWrapper::sig_cursorPositionChanged,
                this, &DesktopAgent::onCursorPositionChanged);
        connect(screen_capturer_, &base::ScreenCapturerWrapper::sig_screenTypeChanged,
                this, &DesktopAgent::onScreenTypeChanged);

        screen_capture_timer_->start(std::chrono::milliseconds(0));
    }

    if (!audio_capturer_)
    {
        audio_capturer_ = new base::AudioCapturerWrapper(this);

        connect(audio_capturer_, &base::AudioCapturerWrapper::sig_audioCaptured, this,
                &DesktopAgent::onAudioCaptured, Qt::QueuedConnection);

        audio_capturer_->start();
    }

    mergeClientConfigurations();
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

    client->disconnect(this); // Disoconnect all signals.
    client->deleteLater();
    clients_.removeOne(client);

    if (!clients_.isEmpty())
        return;

    if (clear_clipboard_)
    {
        if (clipboard_monitor_)
        {
            LOG(INFO) << "Clearing clipboard";
            clipboard_monitor_->clearClipboard();
        }
        else
        {
            LOG(ERROR) << "Clipboard monitor not present";
        }

        clear_clipboard_ = false;
    }

    if (lock_at_disconnect_)
    {
        LOG(INFO) << "Enabled locking of user session when disconnected";

        if (!base::PowerController::lock())
        {
            LOG(ERROR) << "base::PowerController::lock failed";
        }
        else
        {
            LOG(INFO) << "User session locked";
        }

        lock_at_disconnect_ = false;
    }

    if (clipboard_monitor_)
    {
        clipboard_monitor_->disconnect(this);
        clipboard_monitor_->deleteLater();
        clipboard_monitor_ = nullptr;
    }

    if (audio_capturer_)
    {
        audio_capturer_->disconnect(this);
        audio_capturer_->deleteLater();
        audio_capturer_ = nullptr;
    }

    if (screen_capturer_)
    {
        screen_capturer_->disconnect(this);
        screen_capturer_->deleteLater();
        screen_capturer_ = nullptr;
    }

    if (input_injector_)
    {
        input_injector_->disconnect(this);
        input_injector_->deleteLater();
        input_injector_ = nullptr;
    }

    screen_capture_timer_->stop();
    LOG(INFO) << "Session successfully disabled";
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onCaptureScreen()
{
    if (!screen_capturer_)
    {
        LOG(ERROR) << "Screen capturer not initialized";
        return;
    }

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

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onInjectKeyEvent(const proto::desktop::KeyEvent& event)
{
    if (!input_injector_)
    {
        LOG(ERROR) << "Input injector NOT initialized";
        return;
    }

    input_injector_->injectKeyEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onInjectTextEvent(const proto::desktop::TextEvent& event)
{
    if (!input_injector_)
    {
        LOG(ERROR) << "Input injector NOT initialized";
        return;
    }

    input_injector_->injectTextEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onInjectMouseEvent(const proto::desktop::MouseEvent& event)
{
    if (!input_injector_)
    {
        LOG(ERROR) << "Input injector NOT initialized";
        return;
    }

    input_injector_->injectMouseEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onInjectTouchEvent(const proto::desktop::TouchEvent& event)
{
    if (!input_injector_)
    {
        LOG(ERROR) << "Input injector NOT initialized";
        return;
    }

    input_injector_->injectTouchEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onInjectClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (!clipboard_monitor_)
    {
        LOG(ERROR) << "Clipboard monitor NOT initialized";
        return;
    }

    clipboard_monitor_->injectClipboardEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onScreenListChanged(
    const base::ScreenCapturer::ScreenList& list, base::ScreenCapturer::ScreenId current)
{
    proto::desktop::ScreenList screen_list;

    screen_list.set_current_screen(current);

    for (const auto& resolition_item : list.resolutions)
    {
        proto::desktop::Size* resolution = screen_list.add_resolution();
        resolution->set_width(resolition_item.width());
        resolution->set_height(resolition_item.height());
    }

    for (const auto& screen_item : list.screens)
    {
        proto::desktop::Screen* screen = screen_list.add_screen();
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
            screen_list.set_primary_screen(screen_item.id);
    }

    for (const auto& client : std::as_const(clients_))
        client->onScreenListChanged(screen_list);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onCursorPositionChanged(const QPoint& position)
{
    for (const auto& client : std::as_const(clients_))
        client->onCursorPositionChanged(position);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onScreenTypeChanged(base::ScreenCapturer::ScreenType type, const QString& name)
{
    proto::desktop::ScreenType screen_type;
    screen_type.set_name(name.toStdString());

    switch (type)
    {
        case base::ScreenCapturer::ScreenType::DESKTOP:
            screen_type.set_type(proto::desktop::ScreenType::TYPE_DESKTOP);
            break;

        case base::ScreenCapturer::ScreenType::LOCK:
            screen_type.set_type(proto::desktop::ScreenType::TYPE_LOCK);
            break;

        case base::ScreenCapturer::ScreenType::LOGIN:
            screen_type.set_type(proto::desktop::ScreenType::TYPE_LOGIN);
            break;

        case base::ScreenCapturer::ScreenType::OTHER:
            screen_type.set_type(proto::desktop::ScreenType::TYPE_OTHER);
            break;

        default:
            screen_type.set_type(proto::desktop::ScreenType::TYPE_UNKNOWN);
            break;
    }

    for (const auto& client : std::as_const(clients_))
        client->onScreenTypeChanged(screen_type);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    for (const auto& client : std::as_const(clients_))
        client->onClipbaordEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::onAudioCaptured(const proto::desktop::AudioPacket& packet)
{
    for (const auto& client : std::as_const(clients_))
        client->onAudioCaptureData(packet);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgent::mergeClientConfigurations()
{
    if (clients_.isEmpty())
    {
        LOG(INFO) << "No desktop clients";
        return;
    }

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

    if (screen_capturer_)
    {
        screen_capturer_->enableWallpaper(!system_config.disable_wallpaper);
        screen_capturer_->enableEffects(!system_config.disable_effects);
        screen_capturer_->enableFontSmoothing(!system_config.disable_font_smoothing);
        screen_capturer_->enableCursorPosition(system_config.cursor_position);
    }
    else
    {
        LOG(ERROR) << "Screen capturer NOT initialized";
    }

    if (input_injector_)
    {
        input_injector_->setBlockInput(system_config.block_input);
    }
    else
    {
        LOG(ERROR) << "Input injector NOT initialized";
    }

    lock_at_disconnect_ = system_config.lock_at_disconnect;
    clear_clipboard_ = system_config.clear_clipboard;

    onCaptureScreen();
}

} // namespace host
