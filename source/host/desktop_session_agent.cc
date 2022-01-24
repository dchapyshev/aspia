//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/power_controller.h"
#include "base/audio/audio_capturer_wrapper.h"
#include "base/desktop/capture_scheduler.h"
#include "base/desktop/mouse_cursor.h"
#include "base/desktop/screen_capturer_wrapper.h"
#include "base/desktop/shared_frame.h"
#include "base/ipc/shared_memory.h"
#include "base/threading/thread.h"
#include "host/input_injector_win.h"
#include "host/system_settings.h"

namespace host {

namespace {

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

DesktopSessionAgent::DesktopSessionAgent(std::shared_ptr<base::TaskRunner> task_runner)
    : base::ProtobufArena(task_runner),
      task_runner_(task_runner)
{
    LOG(LS_INFO) << "Ctor";

    setArenaStartSize(1 * 1024 * 1024); // 1 MB
    setArenaMaxSize(3 * 1024 * 1024); // 3 MB

    // At the end of the user's session, the program ends later than the others.
    if (!SetProcessShutdownParameters(0, SHUTDOWN_NORETRY))
    {
        PLOG(LS_WARNING) << "SetProcessShutdownParameters failed";
    }

    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
    {
        PLOG(LS_WARNING) << "SetPriorityClass failed";
    }

    SystemSettings settings;
    preferred_video_capturer_ =
        static_cast<base::ScreenCapturer::Type>(settings.preferredVideoCapturer());
    LOG(LS_INFO) << "Preferred video capturer: " << static_cast<int>(preferred_video_capturer_);
}

DesktopSessionAgent::~DesktopSessionAgent()
{
    LOG(LS_INFO) << "Dtor";
}

void DesktopSessionAgent::start(std::u16string_view channel_id)
{
    LOG(LS_INFO) << "Starting with channel id: " << channel_id.data();

    channel_ = std::make_unique<base::IpcChannel>();

    if (!channel_->connect(channel_id))
    {
        LOG(LS_WARNING) << "Connection failed";
        return;
    }

    channel_->setListener(this);
    channel_->resume();
}

void DesktopSessionAgent::onDisconnected()
{
    LOG(LS_INFO) << "IPC channel disconnected";

    setEnabled(false);
    task_runner_->postQuit();
}

void DesktopSessionAgent::onMessageReceived(const base::ByteArray& buffer)
{
    proto::internal::ServiceToDesktop* incoming_message =
        messageFromArena<proto::internal::ServiceToDesktop>();
    incoming_message->Clear();

    if (!base::parse(buffer, incoming_message))
    {
        LOG(LS_ERROR) << "Invalid message from service";
        return;
    }

    if (incoming_message->has_next_screen_capture())
    {
        captureEnd(std::chrono::milliseconds(
            incoming_message->next_screen_capture().update_interval()));
    }
    else if (incoming_message->has_mouse_event())
    {
        if (input_injector_)
            input_injector_->injectMouseEvent(incoming_message->mouse_event());
    }
    else if (incoming_message->has_key_event())
    {
        if (input_injector_)
            input_injector_->injectKeyEvent(incoming_message->key_event());
    }
    else if (incoming_message->has_text_event())
    {
        if (input_injector_)
            input_injector_->injectTextEvent(incoming_message->text_event());
    }
    else if (incoming_message->has_clipboard_event())
    {
        if (clipboard_monitor_)
            clipboard_monitor_->injectClipboardEvent(incoming_message->clipboard_event());
    }
    else if (incoming_message->has_select_source())
    {
        LOG(LS_INFO) << "Select source received";

        if (screen_capturer_)
        {
            const proto::Screen& screen = incoming_message->select_source().screen();
            const proto::Resolution& resolution = screen.resolution();

            screen_capturer_->selectScreen(static_cast<base::ScreenCapturer::ScreenId>(
                screen.id()), base::Size(resolution.width(), resolution.height()));
        }
        else
        {
            LOG(LS_ERROR) << "Screen capturer NOT initialized";
        }
    }
    else if (incoming_message->has_configure())
    {
        const proto::internal::Configure& config = incoming_message->configure();

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
    else if (incoming_message->has_control())
    {
        LOG(LS_INFO) << "Control received: "
                     << controlActionToString(incoming_message->control().action());

        switch (incoming_message->control().action())
        {
            case proto::internal::DesktopControl::ENABLE:
                setEnabled(true);
                break;

            case proto::internal::DesktopControl::DISABLE:
                setEnabled(false);
                break;

            case proto::internal::DesktopControl::LOGOFF:
                base::PowerController::logoff();
                break;

            case proto::internal::DesktopControl::LOCK:
                base::PowerController::lock();
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

void DesktopSessionAgent::onSharedMemoryCreate(int id)
{
    LOG(LS_INFO) << "Shared memory created: " << id;

    proto::internal::DesktopToService* outgoing_message =
        messageFromArena<proto::internal::DesktopToService>();

    proto::internal::SharedBuffer* shared_buffer = outgoing_message->mutable_shared_buffer();
    shared_buffer->set_type(proto::internal::SharedBuffer::CREATE);
    shared_buffer->set_shared_buffer_id(id);

    channel_->send(base::serialize(*outgoing_message));
}

void DesktopSessionAgent::onSharedMemoryDestroy(int id)
{
    LOG(LS_INFO) << "Shared memory destroyed: " << id;

    proto::internal::DesktopToService* outgoing_message =
        messageFromArena<proto::internal::DesktopToService>();

    proto::internal::SharedBuffer* shared_buffer = outgoing_message->mutable_shared_buffer();
    shared_buffer->set_type(proto::internal::SharedBuffer::RELEASE);
    shared_buffer->set_shared_buffer_id(id);

    channel_->send(base::serialize(*outgoing_message));
}

void DesktopSessionAgent::onScreenListChanged(
    const base::ScreenCapturer::ScreenList& list, base::ScreenCapturer::ScreenId current)
{
    proto::internal::DesktopToService* outgoing_message =
        messageFromArena<proto::internal::DesktopToService>();

    proto::ScreenList* screen_list = outgoing_message->mutable_screen_list();
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

        proto::Resolution* resolution = screen->mutable_resolution();
        resolution->set_width(screen_item.resolution.width());
        resolution->set_height(screen_item.resolution.height());

        if (screen_item.is_primary)
            screen_list->set_primary_screen(screen_item.id);
    }

    LOG(LS_INFO) << "Sending screen list to service";
    channel_->send(base::serialize(*outgoing_message));
}

void DesktopSessionAgent::onScreenCaptured(
    const base::Frame* frame, const base::MouseCursor* mouse_cursor)
{
    proto::internal::DesktopToService* outgoing_message =
        messageFromArena<proto::internal::DesktopToService>();

    proto::internal::ScreenCaptured* screen_captured = outgoing_message->mutable_screen_captured();

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
        serialized_frame->set_dpi_x(frame->dpi().x());
        serialized_frame->set_dpi_y(frame->dpi().y());

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
        serialized_mouse_cursor->set_data(base::toStdString(mouse_cursor->constImage()));
    }

    if (screen_captured->has_frame() || screen_captured->has_mouse_cursor())
    {
        channel_->send(base::serialize(*outgoing_message));
    }
    else
    {
        captureEnd(capture_scheduler_->updateInterval());
    }
}

void DesktopSessionAgent::onCursorPositionChanged(const base::Point& position)
{
    proto::internal::DesktopToService* outgoing_message =
        messageFromArena<proto::internal::DesktopToService>();

    proto::CursorPosition* cursor_position = outgoing_message->mutable_cursor_position();
    cursor_position->set_x(position.x());
    cursor_position->set_y(position.y());

    channel_->send(base::serialize(*outgoing_message));
}

void DesktopSessionAgent::onClipboardEvent(const proto::ClipboardEvent& event)
{
    proto::internal::DesktopToService* outgoing_message =
        messageFromArena<proto::internal::DesktopToService>();
    outgoing_message->mutable_clipboard_event()->CopyFrom(event);
    channel_->send(base::serialize(*outgoing_message));
}

void DesktopSessionAgent::setEnabled(bool enable)
{
    LOG(LS_INFO) << "Enable session: " << enable;

    if (enable)
    {
        if (input_injector_)
        {
            LOG(LS_INFO) << "Session already enabled";
            return;
        }

        input_injector_ = std::make_unique<InputInjectorWin>();

        // A window is created to monitor the clipboard. We cannot create windows in the current
        // thread. Create a separate thread.
        clipboard_monitor_ = std::make_unique<common::ClipboardMonitor>();
        clipboard_monitor_->start(task_runner_, this);

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

        task_runner_->postTask(std::bind(&DesktopSessionAgent::captureBegin, shared_from_this()));
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
                LOG(LS_WARNING) << "Clipboard monitor not present";
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

            base::PowerController::lock();
            lock_at_disconnect_ = false;
        }

        LOG(LS_INFO) << "Session successfully disabled";
    }
}

void DesktopSessionAgent::captureBegin()
{
    if (!capture_scheduler_ || !screen_capturer_)
        return;

    capture_scheduler_->beginCapture();
    screen_capturer_->captureFrame();
}

void DesktopSessionAgent::captureEnd(const std::chrono::milliseconds& update_interval)
{
    if (!capture_scheduler_)
    {
        LOG(LS_WARNING) << "No capture scheduler";
        return;
    }

    capture_scheduler_->endCapture();

    if (update_interval == std::chrono::milliseconds::zero())
    {
        // Capture immediately.
        task_runner_->postTask(std::bind(&DesktopSessionAgent::captureBegin, shared_from_this()));
    }
    else
    {
        capture_scheduler_->setUpdateInterval(update_interval);

        task_runner_->postDelayedTask(
            std::bind(&DesktopSessionAgent::captureBegin, shared_from_this()),
            capture_scheduler_->nextCaptureDelay());
    }
}

} // namespace host
