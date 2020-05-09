//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/threading/thread.h"
#include "codec/video_util.h"
#include "desktop/capture_scheduler.h"
#include "desktop/mouse_cursor.h"
#include "desktop/screen_capturer_wrapper.h"
#include "desktop/shared_frame.h"
#include "host/input_injector_win.h"
#include "ipc/shared_memory.h"

namespace host {

DesktopSessionAgent::DesktopSessionAgent(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner))
{
    // At the end of the user's session, the program ends later than the others.
    SetProcessShutdownParameters(0, SHUTDOWN_NORETRY);
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
}

DesktopSessionAgent::~DesktopSessionAgent() = default;

void DesktopSessionAgent::start(std::u16string_view channel_id)
{
    channel_ = std::make_unique<ipc::Channel>();

    if (!channel_->connect(channel_id))
        return;

    channel_->setListener(this);
    channel_->resume();
}

void DesktopSessionAgent::onDisconnected()
{
    setEnabled(false);
    task_runner_->postQuit();
}

void DesktopSessionAgent::onMessageReceived(const base::ByteArray& buffer)
{
    incoming_message_.Clear();

    if (!base::parse(buffer, &incoming_message_))
    {
        LOG(LS_ERROR) << "Invalid message from service";
        return;
    }

    if (incoming_message_.has_capture_screen())
    {
        captureEnd(std::chrono::milliseconds(
            incoming_message_.capture_screen().update_interval()));
    }
    else if (incoming_message_.has_pointer_event())
    {
        if (input_injector_)
            input_injector_->injectPointerEvent(incoming_message_.pointer_event());
    }
    else if (incoming_message_.has_key_event())
    {
        if (input_injector_)
            input_injector_->injectKeyEvent(incoming_message_.key_event());
    }
    else if (incoming_message_.has_clipboard_event())
    {
        if (clipboard_monitor_)
            clipboard_monitor_->injectClipboardEvent(incoming_message_.clipboard_event());
    }
    else if (incoming_message_.has_set_enabled())
    {
        setEnabled(incoming_message_.set_enabled().enable());
    }
    else if (incoming_message_.has_select_source())
    {
        if (screen_capturer_)
            screen_capturer_->selectScreen(incoming_message_.select_source().screen().id());
    }
    else if (incoming_message_.has_set_config())
    {
        const proto::internal::SetConfig& config = incoming_message_.set_config();

        if (screen_capturer_)
        {
            screen_capturer_->enableWallpaper(!config.disable_wallpaper());
            screen_capturer_->enableEffects(!config.disable_effects());
            screen_capturer_->enableFontSmoothing(!config.disable_font_smoothing());
        }

        if (input_injector_)
            input_injector_->setBlockInput(config.block_input());
    }
    else if (incoming_message_.has_desktop_control())
    {
        switch (incoming_message_.desktop_control().action())
        {
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
    outgoing_message_.Clear();

    proto::internal::SharedBuffer* shared_buffer = outgoing_message_.mutable_shared_buffer();
    shared_buffer->set_type(proto::internal::SharedBuffer::CREATE);
    shared_buffer->set_shared_buffer_id(id);

    channel_->send(base::serialize(outgoing_message_));
}

void DesktopSessionAgent::onSharedMemoryDestroy(int id)
{
    outgoing_message_.Clear();

    proto::internal::SharedBuffer* shared_buffer = outgoing_message_.mutable_shared_buffer();
    shared_buffer->set_type(proto::internal::SharedBuffer::RELEASE);
    shared_buffer->set_shared_buffer_id(id);

    channel_->send(base::serialize(outgoing_message_));
}

void DesktopSessionAgent::onScreenListChanged(
    const desktop::ScreenCapturer::ScreenList& list, desktop::ScreenCapturer::ScreenId current)
{
    outgoing_message_.Clear();

    proto::ScreenList* screen_list = outgoing_message_.mutable_screen_list();
    screen_list->set_current_screen(current);

    for (const auto& list_item : list)
    {
        proto::Screen* screen = screen_list->add_screen();
        screen->set_id(list_item.id);
        screen->set_title(list_item.title);

        if (list_item.is_primary)
            screen_list->set_primary_screen(list_item.id);
    }

    channel_->send(base::serialize(outgoing_message_));
}

void DesktopSessionAgent::onScreenCaptured(
    const desktop::Frame* frame, const desktop::MouseCursor* mouse_cursor)
{
    outgoing_message_.Clear();

    proto::internal::ScreenCaptured* screen_captured = outgoing_message_.mutable_screen_captured();

    if (frame && !frame->constUpdatedRegion().isEmpty())
    {
        if (input_injector_)
            input_injector_->setScreenOffset(frame->topLeft());

        proto::internal::DesktopFrame* serialized_frame = screen_captured->mutable_frame();

        serialized_frame->set_shared_buffer_id(frame->sharedMemory()->id());
        serialized_frame->set_width(frame->size().width());
        serialized_frame->set_height(frame->size().height());

        for (desktop::Region::Iterator it(frame->constUpdatedRegion()); !it.isAtEnd(); it.advance())
            codec::serializeRect(it.rect(), serialized_frame->add_dirty_rect());
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
        channel_->send(base::serialize(outgoing_message_));
    }
    else
    {
        captureEnd(capture_scheduler_->updateInterval());
    }
}

void DesktopSessionAgent::onClipboardEvent(const proto::ClipboardEvent& event)
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_clipboard_event()->CopyFrom(event);
    channel_->send(base::serialize(outgoing_message_));
}

void DesktopSessionAgent::setEnabled(bool enable)
{
    if (enable)
    {
        if (input_injector_)
        {
            LOG(LS_INFO) << "Session already started";
            return;
        }

        LOG(LS_INFO) << "Session start...";

        input_injector_ = std::make_unique<InputInjectorWin>();

        // A window is created to monitor the clipboard. We cannot create windows in the current
        // thread. Create a separate thread.
        clipboard_monitor_ = std::make_unique<ClipboardMonitor>();
        clipboard_monitor_->start(task_runner_, this);

        // Create a shared memory factory.
        // We will receive notifications of all creations and destruction of shared memory.
        shared_memory_factory_ = std::make_unique<ipc::SharedMemoryFactory>(this);

        capture_scheduler_ = std::make_unique<desktop::CaptureScheduler>(
            std::chrono::milliseconds(40));

        screen_capturer_ = std::make_unique<desktop::ScreenCapturerWrapper>(this);
        screen_capturer_->setSharedMemoryFactory(shared_memory_factory_.get());

        LOG(LS_INFO) << "Session successfully started";

        task_runner_->postTask(std::bind(&DesktopSessionAgent::captureBegin, shared_from_this()));
    }
    else
    {
        LOG(LS_INFO) << "Session stop...";

        input_injector_.reset();
        capture_scheduler_.reset();
        screen_capturer_.reset();
        shared_memory_factory_.reset();
        clipboard_monitor_.reset();

        LOG(LS_INFO) << "Session successfully stopped";
    }
}

void DesktopSessionAgent::captureBegin()
{
    if (!capture_scheduler_ || !screen_capturer_)
        return;

    capture_scheduler_->beginCapture();
    screen_capturer_->captureFrame();
}

void DesktopSessionAgent::captureEnd(std::chrono::milliseconds update_interval)
{
    if (!capture_scheduler_)
        return;

    capture_scheduler_->endCapture();
    capture_scheduler_->setUpdateInterval(update_interval);

    task_runner_->postDelayedTask(
        std::bind(&DesktopSessionAgent::captureBegin, shared_from_this()),
        capture_scheduler_->nextCaptureDelay());
}

} // namespace host
