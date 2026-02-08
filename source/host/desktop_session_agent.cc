//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/desktop_session_agent.h"

#include <QCoreApplication>

#include "base/logging.h"
#include "base/power_controller.h"
#include "base/audio/audio_capturer_wrapper.h"
#include "base/desktop/mouse_cursor.h"
#include "base/desktop/screen_capturer_wrapper.h"
#include "base/ipc/shared_memory.h"
#include "host/system_settings.h"

#if defined(Q_OS_WINDOWS)
#include "base/desktop/desktop_environment_win.h"
#include "base/win/message_window.h"
#include "host/input_injector_win.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include "host/input_injector_x11.h"
#endif // defined(Q_OS_LINUX)

namespace host {

//--------------------------------------------------------------------------------------------------
DesktopSessionAgent::DesktopSessionAgent(QObject* parent)
    : QObject(parent),
#if defined(Q_OS_WINDOWS)
      ui_thread_(base::Thread::QtDispatcher),
#endif
      screen_capture_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

#if defined(Q_OS_WINDOWS)
    connect(&ui_thread_, &base::Thread::started, this, &DesktopSessionAgent::onBeforeThreadRunning,
            Qt::DirectConnection);
    connect(&ui_thread_, &base::Thread::finished, this, &DesktopSessionAgent::onAfterThreadRunning,
            Qt::DirectConnection);

    // At the end of the user's session, the program ends later than the others.
    if (!SetProcessShutdownParameters(0, SHUTDOWN_NORETRY))
    {
        PLOG(ERROR) << "SetProcessShutdownParameters failed";
    }

    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
    {
        PLOG(ERROR) << "SetPriorityClass failed";
    }
#endif // defined(Q_OS_WINDOWS)

    SystemSettings settings;
    preferred_video_capturer_ =
        static_cast<base::ScreenCapturer::Type>(settings.preferredVideoCapturer());
    LOG(INFO) << "Preferred video capturer:" << static_cast<int>(preferred_video_capturer_);

#if defined(Q_OS_WINDOWS)
    ui_thread_.start();
#endif // defined(Q_OS_WINDOWS)

    screen_capture_timer_->setTimerType(Qt::PreciseTimer);
    connect(screen_capture_timer_, &QTimer::timeout, this, &DesktopSessionAgent::captureScreen);
}

//--------------------------------------------------------------------------------------------------
DesktopSessionAgent::~DesktopSessionAgent()
{
    LOG(INFO) << "Dtor";

#if defined(Q_OS_WINDOWS)
    ui_thread_.stop();
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::start(const QString& channel_id)
{
    LOG(INFO) << "Starting (channel_id=" << channel_id << ")";

    ipc_channel_ = new base::IpcChannel(this);

    if (!ipc_channel_->connectTo(channel_id))
    {
        LOG(ERROR) << "Connection failed (channel_id=" << channel_id << ")";
        return;
    }

    connect(ipc_channel_, &base::IpcChannel::sig_disconnected,
            this, &DesktopSessionAgent::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived,
            this, &DesktopSessionAgent::onIpcMessageReceived);

    ipc_channel_->resume();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onBeforeThreadRunning()
{
    LOG(INFO) << "UI thread starting";

#if defined(Q_OS_WINDOWS)
    message_window_ = std::make_unique<base::MessageWindow>();
    if (!message_window_->create(std::bind(&DesktopSessionAgent::onWindowsMessage,
                                           this,
                                           std::placeholders::_1, std::placeholders::_2,
                                           std::placeholders::_3, std::placeholders::_4)))
    {
        LOG(ERROR) << "Couldn't create window";
        return;
    }
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onAfterThreadRunning()
{
    LOG(INFO) << "UI thread stopping";

#if defined(Q_OS_WINDOWS)
    message_window_.reset();
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onSharedMemoryCreate(int id)
{
    LOG(INFO) << "Shared memory created:" << id;

    proto::internal::SharedBuffer* shared_buffer =
        outgoing_message_.newMessage().mutable_shared_buffer();

    shared_buffer->set_type(proto::internal::SharedBuffer::CREATE);
    shared_buffer->set_shared_buffer_id(id);

    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onSharedMemoryDestroy(int id)
{
    LOG(INFO) << "Shared memory destroyed:" << id;

    proto::internal::SharedBuffer* shared_buffer =
        outgoing_message_.newMessage().mutable_shared_buffer();

    shared_buffer->set_type(proto::internal::SharedBuffer::RELEASE);
    shared_buffer->set_shared_buffer_id(id);

    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onScreenListChanged(
    const base::ScreenCapturer::ScreenList& list, base::ScreenCapturer::ScreenId current)
{
    proto::desktop::ScreenList* screen_list = outgoing_message_.newMessage().mutable_screen_list();
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

    LOG(INFO) << "Sending screen list to service";
    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onCursorPositionChanged(const QPoint& position)
{
    proto::desktop::CursorPosition* cursor_position =
        outgoing_message_.newMessage().mutable_cursor_position();

    cursor_position->set_x(position.x());
    cursor_position->set_y(position.y());

    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onScreenTypeChanged(
    base::ScreenCapturer::ScreenType type, const QString& name)
{
    proto::desktop::ScreenType* screen_type = outgoing_message_.newMessage().mutable_screen_type();
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

    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onIpcDisconnected()
{
    LOG(INFO) << "IPC channel disconnected";

    setEnabled(false);

    LOG(INFO) << "Post quit";
    QCoreApplication::quit();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onIpcMessageReceived(const QByteArray& buffer)
{
    if (!incoming_message_.parse(buffer))
    {
        LOG(ERROR) << "Invalid message from service";
        return;
    }

    if (incoming_message_->has_next_screen_capture())
    {
        scheduleNextCapture(std::chrono::milliseconds(
            incoming_message_->next_screen_capture().update_interval()));
    }
    else if (incoming_message_->has_mouse_event())
    {
        if (input_injector_)
        {
            input_injector_->injectMouseEvent(incoming_message_->mouse_event());
        }
        else
        {
            LOG(ERROR) << "Input injector NOT initialized";
        }
    }
    else if (incoming_message_->has_key_event())
    {
        if (input_injector_)
        {
            input_injector_->injectKeyEvent(incoming_message_->key_event());
        }
        else
        {
            LOG(ERROR) << "Input injector NOT initialized";
        }
    }
    else if (incoming_message_->has_touch_event())
    {
        if (input_injector_)
        {
            input_injector_->injectTouchEvent(incoming_message_->touch_event());
        }
        else
        {
            LOG(ERROR) << "Input injector NOT initialized";
        }
    }
    else if (incoming_message_->has_text_event())
    {
        if (input_injector_)
        {
            input_injector_->injectTextEvent(incoming_message_->text_event());
        }
        else
        {
            LOG(ERROR) << "Input injector NOT initialized";
        }
    }
    else if (incoming_message_->has_clipboard_event())
    {
        if (clipboard_monitor_)
        {
            clipboard_monitor_->injectClipboardEvent(incoming_message_->clipboard_event());
        }
        else
        {
            LOG(ERROR) << "Clipboard monitor NOT initialized";
        }
    }
    else if (incoming_message_->has_select_source())
    {
        LOG(INFO) << "Select source received";

        if (screen_capturer_)
        {
            const proto::desktop::Screen& screen = incoming_message_->select_source().screen();
            const proto::desktop::Size& resolution = screen.resolution();

            screen_capturer_->selectScreen(static_cast<base::ScreenCapturer::ScreenId>(
                                           screen.id()), base::parse(resolution));
        }
        else
        {
            LOG(ERROR) << "Screen capturer NOT initialized";
        }
    }
    else if (incoming_message_->has_configure())
    {
        const proto::internal::Configure& config = incoming_message_->configure();

        LOG(INFO) << "Configure received";
        LOG(INFO) << "Disable wallpaper:" << config.disable_wallpaper();
        LOG(INFO) << "Disable effects:" << config.disable_effects();
        LOG(INFO) << "Disable font smoothing:" << config.disable_font_smoothing();
        LOG(INFO) << "Block input:" << config.block_input();
        LOG(INFO) << "Lock at disconnect:" << config.lock_at_disconnect();
        LOG(INFO) << "Clear clipboard:" << config.clear_clipboard();
        LOG(INFO) << "Cursor position:" << config.cursor_position();

        if (screen_capturer_)
        {
            screen_capturer_->enableWallpaper(!config.disable_wallpaper());
            screen_capturer_->enableEffects(!config.disable_effects());
            screen_capturer_->enableFontSmoothing(!config.disable_font_smoothing());
            screen_capturer_->enableCursorPosition(config.cursor_position());
        }
        else
        {
            LOG(ERROR) << "Screen capturer NOT initialized";
        }

        if (input_injector_)
        {
            input_injector_->setBlockInput(config.block_input());
        }
        else
        {
            LOG(ERROR) << "Input injector NOT initialized";
        }

        lock_at_disconnect_ = config.lock_at_disconnect();
        clear_clipboard_ = config.clear_clipboard();
    }
    else if (incoming_message_->has_control())
    {
        LOG(INFO) << "Control received:" << incoming_message_->control().action();

        switch (incoming_message_->control().action())
        {
            case proto::internal::DesktopControl::ENABLE:
                setEnabled(true);
                break;

            case proto::internal::DesktopControl::DISABLE:
                setEnabled(false);
                break;

            case proto::internal::DesktopControl::LOGOFF:
            {
                if (!base::PowerController::logoff())
                {
                    LOG(ERROR) << "base::PowerController::logoff failed";
                }
            }
            break;

            case proto::internal::DesktopControl::LOCK:
            {
                if (!base::PowerController::lock())
                {
                    LOG(ERROR) << "base::PowerController::lock failed";
                }
            }
            break;

            default:
                NOTREACHED();
                break;
        }
    }
    else
    {
        LOG(ERROR) << "Unhandled message from service";
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    LOG(INFO) << "Send clipboard event";

    outgoing_message_.newMessage().mutable_clipboard_event()->CopyFrom(event);
    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::setEnabled(bool enable)
{
    LOG(INFO) << "Enable session: " << enable;

    if (is_session_enabled_ == enable)
    {
        LOG(INFO) << "Session already" << (enable ? "enabled" : "disabled");
        return;
    }

    is_session_enabled_ = enable;
    if (enable)
    {
#if defined(Q_OS_WINDOWS)
        input_injector_ = new InputInjectorWin(this);
#elif defined(Q_OS_LINUX)
        input_injector_ = InputInjectorX11::create();
#else
        LOG(LS_ERROR) << "Input injector not supported for platform";
#endif

        // A window is created to monitor the clipboard. We cannot create windows in the current
        // thread. Create a separate thread.
        clipboard_monitor_ = new common::ClipboardMonitor(this);
        connect(clipboard_monitor_, &common::ClipboardMonitor::sig_clipboardEvent,
                this, &DesktopSessionAgent::onClipboardEvent);
        clipboard_monitor_->start();

        // Create a shared memory factory.
        // We will receive notifications of all creations and destruction of shared memory.
        shared_memory_factory_ = new base::SharedMemoryFactory(this);

        connect(shared_memory_factory_, &base::SharedMemoryFactory::sig_memoryCreated,
                this, &DesktopSessionAgent::onSharedMemoryCreate);
        connect(shared_memory_factory_, &base::SharedMemoryFactory::sig_memoryDestroyed,
                this, &DesktopSessionAgent::onSharedMemoryDestroy);

        capture_scheduler_.setUpdateInterval(std::chrono::milliseconds(40));

        screen_capturer_ = new base::ScreenCapturerWrapper(preferred_video_capturer_, this);
        screen_capturer_->setSharedMemoryFactory(shared_memory_factory_);

        connect(screen_capturer_, &base::ScreenCapturerWrapper::sig_screenListChanged,
                this, &DesktopSessionAgent::onScreenListChanged);
        connect(screen_capturer_, &base::ScreenCapturerWrapper::sig_cursorPositionChanged,
                this, &DesktopSessionAgent::onCursorPositionChanged);
        connect(screen_capturer_, &base::ScreenCapturerWrapper::sig_screenTypeChanged,
                this, &DesktopSessionAgent::onScreenTypeChanged);

        audio_capturer_ = new base::AudioCapturerWrapper(this);

        connect(audio_capturer_, &base::AudioCapturerWrapper::sig_sendMessage, this,
                [this](const QByteArray& data)
        {
            ipc_channel_->send(QByteArray(data));
        }, Qt::QueuedConnection);

        audio_capturer_->start();

        LOG(INFO) << "Session successfully enabled";

        screen_capture_timer_->start(std::chrono::milliseconds(0));
    }
    else
    {
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

        delete input_injector_;
        delete screen_capturer_;
        delete shared_memory_factory_;
        delete clipboard_monitor_;
        delete audio_capturer_;

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

        screen_capture_timer_->stop();
        LOG(INFO) << "Session successfully disabled";
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::captureScreen()
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

    proto::internal::ScreenCaptured* screen_captured =
        outgoing_message_.newMessage().mutable_screen_captured();

    if (error != base::ScreenCapturer::Error::SUCCEEDED)
    {
        switch (error)
        {
        case base::ScreenCapturer::Error::PERMANENT:
            screen_captured->set_error_code(proto::desktop::VIDEO_ERROR_CODE_PERMANENT);
            break;

        case base::ScreenCapturer::Error::TEMPORARY:
            screen_captured->set_error_code(proto::desktop::VIDEO_ERROR_CODE_TEMPORARY);
            break;

        default:
            NOTREACHED();
            return;
        }

        ipc_channel_->send(outgoing_message_.serialize());
        return;
    }

    if (frame && !frame->constUpdatedRegion().isEmpty())
    {
        base::SharedMemory* shared_memory = frame->sharedMemory();
        if (!shared_memory)
        {
            LOG(ERROR) << "Unable to get shared memory";
            return;
        }

        if (input_injector_)
            input_injector_->setScreenOffset(frame->topLeft());

        proto::internal::DesktopFrame* serialized_frame = screen_captured->mutable_frame();

        serialized_frame->set_capturer_type(frame->capturerType());
        serialized_frame->set_shared_buffer_id(shared_memory->id());
        serialized_frame->set_width(frame->size().width());
        serialized_frame->set_height(frame->size().height());

        for (const auto& rect : frame->constUpdatedRegion())
        {
            proto::desktop::Rect* dirty_rect = serialized_frame->add_dirty_rect();
            dirty_rect->set_x(rect.x());
            dirty_rect->set_y(rect.y());
            dirty_rect->set_width(rect.width());
            dirty_rect->set_height(rect.height());
        }
    }

    if (cursor)
    {
        proto::internal::MouseCursor* serialized_mouse_cursor =
            screen_captured->mutable_mouse_cursor();

        serialized_mouse_cursor->set_width(cursor->width());
        serialized_mouse_cursor->set_height(cursor->height());
        serialized_mouse_cursor->set_hotspot_x(cursor->hotSpotX());
        serialized_mouse_cursor->set_hotspot_y(cursor->hotSpotY());
        serialized_mouse_cursor->set_dpi_x(cursor->constDpi().x());
        serialized_mouse_cursor->set_dpi_y(cursor->constDpi().y());
        serialized_mouse_cursor->set_data(cursor->constImage().toStdString());
    }

    if (screen_captured->has_frame() || screen_captured->has_mouse_cursor())
    {
        // If a frame or cursor was captured, send a message to the service. The next capture
        // will be scheduled after the response from the service.
        ipc_channel_->send(outgoing_message_.serialize());
    }
    else
    {
        // Frame and cursor unchanged. Plan the next capture immediately.
        scheduleNextCapture(capture_scheduler_.updateInterval());
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::scheduleNextCapture(const std::chrono::milliseconds& update_interval)
{
    capture_scheduler_.onEndCapture();

    if (update_interval == std::chrono::milliseconds::zero())
    {
        // Capture immediately.
        screen_capture_timer_->start(std::chrono::milliseconds(0));
    }
    else
    {
        capture_scheduler_.setUpdateInterval(update_interval);
        screen_capture_timer_->start(capture_scheduler_.nextCaptureDelay());
    }
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
bool DesktopSessionAgent::onWindowsMessage(
    UINT message, WPARAM /* wparam */, LPARAM /* lparam */, LRESULT& result)
{
    switch (message)
    {
        case WM_QUERYENDSESSION:
        {
            LOG(INFO) << "WM_QUERYENDSESSION received";

            base::DesktopEnvironmentWin::updateEnvironment();

            result = TRUE;
            return true;
        }

        case WM_ENDSESSION:
        {
            LOG(INFO) << "WM_ENDSESSION received";
            result = FALSE;
            return true;
        }

        default:
            break;
    }

    return false;
}
#endif // defined(Q_OS_WINDOWS)

} // namespace host
