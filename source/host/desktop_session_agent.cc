//
// Aspia Project
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

#include "base/logging.h"
#include "base/serialization.h"
#include "base/power_controller.h"
#include "base/audio/audio_capturer_wrapper.h"
#include "base/desktop/capture_scheduler.h"
#include "base/desktop/mouse_cursor.h"
#include "base/desktop/screen_capturer_wrapper.h"
#include "base/desktop/shared_frame.h"
#include "base/ipc/shared_memory.h"
#include "host/system_settings.h"

#if defined(OS_WIN)
#include "base/desktop/desktop_environment_win.h"
#include "base/win/message_window.h"
#include "host/input_injector_win.h"
#endif // defined(OS_WIN)

#if defined(OS_LINUX)
#include "host/input_injector_x11.h"
#endif // defined(OS_LINUX)

#include <QCoreApplication>

namespace host {

namespace {

//--------------------------------------------------------------------------------------------------
const char* controlActionToString(proto::internal::DesktopControl::Action action)
{
    switch (action)
    {
        case proto::internal::DesktopControl::ENABLE:
            return "ENABLE";

        case proto::internal::DesktopControl::DISABLE:
            return "DISABLE";

        case proto::internal::DesktopControl::LOCK:
            return "LOCK";

        case proto::internal::DesktopControl::LOGOFF:
            return "LOGOFF";

        default:
            return "Unknown control action";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopSessionAgent::DesktopSessionAgent(QObject* parent)
    : QObject(parent),
      ui_thread_(base::Thread::QtDispatcher, this),
      screen_capture_timer_(new QTimer(this))
{
    LOG(LS_INFO) << "Ctor";

#if defined(OS_WIN)
    // At the end of the user's session, the program ends later than the others.
    if (!SetProcessShutdownParameters(0, SHUTDOWN_NORETRY))
    {
        PLOG(LS_ERROR) << "SetProcessShutdownParameters failed";
    }

    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
    {
        PLOG(LS_ERROR) << "SetPriorityClass failed";
    }
#endif // defined(OS_WIN)

    SystemSettings settings;
    preferred_video_capturer_ =
        static_cast<base::ScreenCapturer::Type>(settings.preferredVideoCapturer());
    LOG(LS_INFO) << "Preferred video capturer: " << static_cast<int>(preferred_video_capturer_);

#if defined(OS_WIN)
    ui_thread_.start();
#endif // defined(OS_WIN)

    screen_capture_timer_->setTimerType(Qt::PreciseTimer);
    connect(screen_capture_timer_, &QTimer::timeout, this, &DesktopSessionAgent::captureBegin);
}

//--------------------------------------------------------------------------------------------------
DesktopSessionAgent::~DesktopSessionAgent()
{
    LOG(LS_INFO) << "Dtor";

#if defined(OS_WIN)
    ui_thread_.stop();
#endif // defined(OS_WIN)
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::start(const QString& channel_id)
{
    LOG(LS_INFO) << "Starting (channel_id=" << channel_id.data() << ")";

    channel_ = std::make_unique<base::IpcChannel>();

    if (!channel_->connect(channel_id))
    {
        LOG(LS_ERROR) << "Connection failed (channel_id=" << channel_id.data() << ")";
        return;
    }

    channel_->setListener(this);
    channel_->resume();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onIpcDisconnected()
{
    LOG(LS_INFO) << "IPC channel disconnected";

    setEnabled(false);

    LOG(LS_INFO) << "Post quit";
    QCoreApplication::quit();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onIpcMessageReceived(const QByteArray& buffer)
{
    incoming_message_.Clear();

    if (!base::parse(buffer, &incoming_message_))
    {
        LOG(LS_ERROR) << "Invalid message from service";
        return;
    }

    if (incoming_message_.has_next_screen_capture())
    {
        captureEnd(std::chrono::milliseconds(
            incoming_message_.next_screen_capture().update_interval()));
    }
    else if (incoming_message_.has_mouse_event())
    {
        if (input_injector_)
        {
            input_injector_->injectMouseEvent(incoming_message_.mouse_event());
        }
        else
        {
            LOG(LS_ERROR) << "Input injector NOT initialized";
        }
    }
    else if (incoming_message_.has_key_event())
    {
        if (input_injector_)
        {
            input_injector_->injectKeyEvent(incoming_message_.key_event());
        }
        else
        {
            LOG(LS_ERROR) << "Input injector NOT initialized";
        }
    }
    else if (incoming_message_.has_touch_event())
    {
        if (input_injector_)
        {
            input_injector_->injectTouchEvent(incoming_message_.touch_event());
        }
        else
        {
            LOG(LS_ERROR) << "Input injector NOT initialized";
        }
    }
    else if (incoming_message_.has_text_event())
    {
        if (input_injector_)
        {
            input_injector_->injectTextEvent(incoming_message_.text_event());
        }
        else
        {
            LOG(LS_ERROR) << "Input injector NOT initialized";
        }
    }
    else if (incoming_message_.has_clipboard_event())
    {
        if (clipboard_monitor_)
        {
            clipboard_monitor_->injectClipboardEvent(incoming_message_.clipboard_event());
        }
        else
        {
            LOG(LS_ERROR) << "Clipboard monitor NOT initialized";
        }
    }
    else if (incoming_message_.has_select_source())
    {
        LOG(LS_INFO) << "Select source received";

        if (screen_capturer_)
        {
            const proto::Screen& screen = incoming_message_.select_source().screen();
            const proto::Resolution& resolution = screen.resolution();

            screen_capturer_->selectScreen(static_cast<base::ScreenCapturer::ScreenId>(
                screen.id()), base::Size(resolution.width(), resolution.height()));
        }
        else
        {
            LOG(LS_ERROR) << "Screen capturer NOT initialized";
        }
    }
    else if (incoming_message_.has_configure())
    {
        const proto::internal::Configure& config = incoming_message_.configure();

        LOG(LS_INFO) << "Configure received";
        LOG(LS_INFO) << "Disable wallpaper: " << config.disable_wallpaper();
        LOG(LS_INFO) << "Disable effects: " << config.disable_effects();
        LOG(LS_INFO) << "Disable font smoothing: " << config.disable_font_smoothing();
        LOG(LS_INFO) << "Block input: " << config.block_input();
        LOG(LS_INFO) << "Lock at disconnect: " << config.lock_at_disconnect();
        LOG(LS_INFO) << "Clear clipboard: " << config.clear_clipboard();
        LOG(LS_INFO) << "Cursor position: " << config.cursor_position();

        if (screen_capturer_)
        {
            screen_capturer_->enableWallpaper(!config.disable_wallpaper());
            screen_capturer_->enableEffects(!config.disable_effects());
            screen_capturer_->enableFontSmoothing(!config.disable_font_smoothing());
            screen_capturer_->enableCursorPosition(config.cursor_position());
        }
        else
        {
            LOG(LS_ERROR) << "Screen capturer NOT initialized";
        }

        if (input_injector_)
        {
            input_injector_->setBlockInput(config.block_input());
        }
        else
        {
            LOG(LS_ERROR) << "Input injector NOT initialized";
        }

        lock_at_disconnect_ = config.lock_at_disconnect();
        clear_clipboard_ = config.clear_clipboard();
    }
    else if (incoming_message_.has_control())
    {
        LOG(LS_INFO) << "Control received: "
                     << controlActionToString(incoming_message_.control().action());

        switch (incoming_message_.control().action())
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
                    LOG(LS_ERROR) << "base::PowerController::logoff failed";
                }
            }
            break;

            case proto::internal::DesktopControl::LOCK:
            {
                if (!base::PowerController::lock())
                {
                    LOG(LS_ERROR) << "base::PowerController::lock failed";
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
        LOG(LS_ERROR) << "Unhandled message from service";
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onIpcMessageWritten()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onSharedMemoryCreate(int id)
{
    LOG(LS_INFO) << "Shared memory created: " << id;

    outgoing_message_.Clear();

    proto::internal::SharedBuffer* shared_buffer = outgoing_message_.mutable_shared_buffer();
    shared_buffer->set_type(proto::internal::SharedBuffer::CREATE);
    shared_buffer->set_shared_buffer_id(id);

    channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onSharedMemoryDestroy(int id)
{
    LOG(LS_INFO) << "Shared memory destroyed: " << id;

    outgoing_message_.Clear();

    proto::internal::SharedBuffer* shared_buffer = outgoing_message_.mutable_shared_buffer();
    shared_buffer->set_type(proto::internal::SharedBuffer::RELEASE);
    shared_buffer->set_shared_buffer_id(id);

    channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onScreenListChanged(
    const base::ScreenCapturer::ScreenList& list, base::ScreenCapturer::ScreenId current)
{
    outgoing_message_.Clear();

    proto::ScreenList* screen_list = outgoing_message_.mutable_screen_list();
    screen_list->set_current_screen(current);

    for (const auto& resolition_item : list.resolutions)
    {
        proto::Resolution* resolution = screen_list->add_resolution();
        resolution->set_width(resolition_item.width());
        resolution->set_height(resolition_item.height());
    }

    for (const auto& screen_item : list.screens)
    {
        proto::Screen* screen = screen_list->add_screen();
        screen->set_id(screen_item.id);
        screen->set_title(screen_item.title);

        proto::Point* position = screen->mutable_position();
        position->set_x(screen_item.position.x());
        position->set_y(screen_item.position.y());

        proto::Resolution* resolution = screen->mutable_resolution();
        resolution->set_width(screen_item.resolution.width());
        resolution->set_height(screen_item.resolution.height());

        proto::Point* dpi = screen->mutable_dpi();
        dpi->set_x(screen_item.dpi.x());
        dpi->set_y(screen_item.dpi.y());

        if (screen_item.is_primary)
            screen_list->set_primary_screen(screen_item.id);
    }

    LOG(LS_INFO) << "Sending screen list to service";
    channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onScreenCaptured(
    const base::Frame* frame, const base::MouseCursor* mouse_cursor)
{
    outgoing_message_.Clear();

    proto::internal::ScreenCaptured* screen_captured = outgoing_message_.mutable_screen_captured();

    if (frame && !frame->constUpdatedRegion().isEmpty())
    {
        base::SharedMemoryBase* shared_memory = frame->sharedMemory();
        if (!shared_memory)
        {
            LOG(LS_ERROR) << "Unable to get shared memory";
            return;
        }

        if (input_injector_)
            input_injector_->setScreenOffset(frame->topLeft());

        proto::internal::DesktopFrame* serialized_frame = screen_captured->mutable_frame();

        serialized_frame->set_capturer_type(frame->capturerType());
        serialized_frame->set_shared_buffer_id(shared_memory->id());
        serialized_frame->set_width(frame->size().width());
        serialized_frame->set_height(frame->size().height());

        for (base::Region::Iterator it(frame->constUpdatedRegion()); !it.isAtEnd(); it.advance())
        {
            proto::Rect* dirty_rect = serialized_frame->add_dirty_rect();
            base::Rect rect = it.rect();

            dirty_rect->set_x(rect.x());
            dirty_rect->set_y(rect.y());
            dirty_rect->set_width(rect.width());
            dirty_rect->set_height(rect.height());
        }
    }

    if (mouse_cursor)
    {
        proto::internal::MouseCursor* serialized_mouse_cursor =
            screen_captured->mutable_mouse_cursor();

        serialized_mouse_cursor->set_width(mouse_cursor->width());
        serialized_mouse_cursor->set_height(mouse_cursor->height());
        serialized_mouse_cursor->set_hotspot_x(mouse_cursor->hotSpotX());
        serialized_mouse_cursor->set_hotspot_y(mouse_cursor->hotSpotY());
        serialized_mouse_cursor->set_dpi_x(mouse_cursor->constDpi().x());
        serialized_mouse_cursor->set_dpi_y(mouse_cursor->constDpi().y());
        serialized_mouse_cursor->set_data(mouse_cursor->constImage().toStdString());
    }

    if (screen_captured->has_frame() || screen_captured->has_mouse_cursor())
    {
        channel_->send(base::serialize(outgoing_message_));
    }
    else
    {
        captureEnd(capture_scheduler_->updateInterval());
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onScreenCaptureError(base::ScreenCapturer::Error error)
{
    outgoing_message_.Clear();
    proto::internal::ScreenCaptured* screen_captured = outgoing_message_.mutable_screen_captured();

    switch (error)
    {
        case base::ScreenCapturer::Error::PERMANENT:
            screen_captured->set_error_code(proto::VIDEO_ERROR_CODE_PERMANENT);
            break;

        case base::ScreenCapturer::Error::TEMPORARY:
            screen_captured->set_error_code(proto::VIDEO_ERROR_CODE_TEMPORARY);
            break;

        default:
            NOTREACHED();
            return;
    }

    channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onCursorPositionChanged(const base::Point& position)
{
    outgoing_message_.Clear();

    proto::CursorPosition* cursor_position = outgoing_message_.mutable_cursor_position();
    cursor_position->set_x(position.x());
    cursor_position->set_y(position.y());

    channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onScreenTypeChanged(
    base::ScreenCapturer::ScreenType type, const std::string& name)
{
    outgoing_message_.Clear();

    proto::ScreenType* screen_type = outgoing_message_.mutable_screen_type();
    screen_type->set_name(name);

    switch (type)
    {
        case base::ScreenCapturer::ScreenType::DESKTOP:
            screen_type->set_type(proto::ScreenType::TYPE_DESKTOP);
            break;

        case base::ScreenCapturer::ScreenType::LOCK:
            screen_type->set_type(proto::ScreenType::TYPE_LOCK);
            break;

        case base::ScreenCapturer::ScreenType::LOGIN:
            screen_type->set_type(proto::ScreenType::TYPE_LOGIN);
            break;

        case base::ScreenCapturer::ScreenType::OTHER:
            screen_type->set_type(proto::ScreenType::TYPE_OTHER);
            break;

        default:
            screen_type->set_type(proto::ScreenType::TYPE_UNKNOWN);
            break;
    }

    channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onBeforeThreadRunning()
{
    LOG(LS_INFO) << "UI thread starting";

#if defined(OS_WIN)
    message_window_ = std::make_unique<base::win::MessageWindow>();
    if (!message_window_->create(std::bind(&DesktopSessionAgent::onWindowsMessage,
                                           this,
                                           std::placeholders::_1, std::placeholders::_2,
                                           std::placeholders::_3, std::placeholders::_4)))
    {
        LOG(LS_ERROR) << "Couldn't create window.";
        return;
    }
#endif // defined(OS_WIN)
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onAfterThreadRunning()
{
    LOG(LS_INFO) << "UI thread stopping";

#if defined(OS_WIN)
    message_window_.reset();
#endif // defined(OS_WIN)
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::onClipboardEvent(const proto::ClipboardEvent& event)
{
    LOG(LS_INFO) << "Send clipboard event";

    outgoing_message_.Clear();
    outgoing_message_.mutable_clipboard_event()->CopyFrom(event);
    channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::setEnabled(bool enable)
{
    LOG(LS_INFO) << "Enable session: " << enable;

    if (is_session_enabled_ == enable)
    {
        LOG(LS_INFO) << "Session already " << (enable ? "enabled" : "disabled");
        return;
    }

    is_session_enabled_ = enable;
    if (enable)
    {
#if defined(OS_WIN)
        input_injector_ = std::make_unique<InputInjectorWin>();
#elif defined(OS_LINUX)
        input_injector_ = InputInjectorX11::create();
#else
        LOG(LS_ERROR) << "Input injector not supported for platform";
#endif

        // A window is created to monitor the clipboard. We cannot create windows in the current
        // thread. Create a separate thread.
        clipboard_monitor_ = std::make_unique<common::ClipboardMonitor>();
        connect(clipboard_monitor_.get(), &common::ClipboardMonitor::sig_clipboardEvent,
                this, &DesktopSessionAgent::onClipboardEvent);
        clipboard_monitor_->start();

        // Create a shared memory factory.
        // We will receive notifications of all creations and destruction of shared memory.
        shared_memory_factory_ = std::make_unique<base::SharedMemoryFactory>(this);

        capture_scheduler_ = std::make_unique<base::CaptureScheduler>(
            std::chrono::milliseconds(40));

        screen_capturer_ = std::make_unique<base::ScreenCapturerWrapper>(
            preferred_video_capturer_, this);
        screen_capturer_->setSharedMemoryFactory(shared_memory_factory_.get());

        audio_capturer_ = std::make_unique<base::AudioCapturerWrapper>(channel_->channelProxy());
        audio_capturer_->start();

        LOG(LS_INFO) << "Session successfully enabled";

        screen_capture_timer_->start(std::chrono::milliseconds(0));
    }
    else
    {
        if (clear_clipboard_)
        {
            if (clipboard_monitor_)
            {
                LOG(LS_INFO) << "Clearing clipboard";
                clipboard_monitor_->clearClipboard();
            }
            else
            {
                LOG(LS_ERROR) << "Clipboard monitor not present";
            }

            clear_clipboard_ = false;
        }

        input_injector_.reset();
        capture_scheduler_.reset();
        screen_capturer_.reset();
        shared_memory_factory_.reset();
        clipboard_monitor_.reset();
        audio_capturer_.reset();

        if (lock_at_disconnect_)
        {
            LOG(LS_INFO) << "Enabled locking of user session when disconnected";

            if (!base::PowerController::lock())
            {
                LOG(LS_ERROR) << "base::PowerController::lock failed";
            }
            else
            {
                LOG(LS_INFO) << "User session locked";
            }

            lock_at_disconnect_ = false;
        }

        screen_capture_timer_->stop();
        LOG(LS_INFO) << "Session successfully disabled";
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::captureBegin()
{
    if (!capture_scheduler_ || !screen_capturer_)
    {
        LOG(LS_ERROR) << "Screen capturer not initialized";
        return;
    }

    capture_scheduler_->beginCapture();
    screen_capturer_->captureFrame();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionAgent::captureEnd(const std::chrono::milliseconds& update_interval)
{
    if (!capture_scheduler_)
    {
        LOG(LS_ERROR) << "No capture scheduler";
        return;
    }

    capture_scheduler_->endCapture();

    if (update_interval == std::chrono::milliseconds::zero())
    {
        // Capture immediately.
        screen_capture_timer_->start(std::chrono::milliseconds(0));
    }
    else
    {
        capture_scheduler_->setUpdateInterval(update_interval);
        screen_capture_timer_->start(capture_scheduler_->nextCaptureDelay());
    }
}

#if defined(OS_WIN)
//--------------------------------------------------------------------------------------------------
bool DesktopSessionAgent::onWindowsMessage(
    UINT message, WPARAM /* wparam */, LPARAM /* lparam */, LRESULT& result)
{
    switch (message)
    {
        case WM_QUERYENDSESSION:
        {
            LOG(LS_INFO) << "WM_QUERYENDSESSION received";

            base::DesktopEnvironmentWin::updateEnvironment();

            result = TRUE;
            return true;
        }

        case WM_ENDSESSION:
        {
            LOG(LS_INFO) << "WM_ENDSESSION received";
            result = FALSE;
            return true;
        }

        default:
            break;
    }

    return false;
}
#endif // defined(OS_WIN)

} // namespace host
