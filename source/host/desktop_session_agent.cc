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
#include "base/threading/thread.h"
#include "codec/video_util.h"
#include "common/message_serialization.h"
#include "desktop/screen_capturer_wrapper.h"
#include "desktop/shared_desktop_frame.h"
#include "host/input_injector_win.h"
#include "ipc/ipc_channel.h"
#include "ipc/shared_memory.h"

namespace host {

DesktopSessionAgent::DesktopSessionAgent(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner))
{
    // Nothing
}

DesktopSessionAgent::~DesktopSessionAgent() = default;

void DesktopSessionAgent::start(std::u16string_view channel_id)
{
    channel_ = std::make_unique<ipc::Channel>();

    if (!channel_->connect(channel_id))
        return;

    channel_->setListener(this);
    channel_->resume();

    // A window is created to monitor the clipboard. We cannot create windows in the current
    // thread. Create a separate thread.
    //clipboard_thread_ = std::make_unique<base::Thread>();
    //clipboard_thread_->start(base::MessageLoop::Type::WIN);

    //std::shared_ptr<base::TaskRunner> clipboard_task_runner = clipboard_thread_->taskRunner();

    // Create a shared memory factory.
    // We will receive notifications of all creations and destruction of shared memory.
    shared_memory_factory_ = std::make_unique<ipc::SharedMemoryFactory>(this);

    screen_capturer_ = std::make_unique<desktop::ScreenCapturerWrapper>(this);
    screen_capturer_->setSharedMemoryFactory(shared_memory_factory_.get());

    input_injector_ = std::make_unique<InputInjectorWin>();
}

void DesktopSessionAgent::onDisconnected()
{
    input_injector_.reset();
    screen_capturer_.reset();
    shared_memory_factory_.reset();
    clipboard_thread_.reset();

    task_runner_->postQuit();
}

void DesktopSessionAgent::onMessageReceived(const base::ByteArray& buffer)
{
    incoming_message_.Clear();

    if (!common::parseMessage(buffer, &incoming_message_))
    {
        LOG(LS_ERROR) << "Invalid message from service";
        return;
    }

    if (incoming_message_.has_select_source())
    {
        screen_capturer_->selectScreen(incoming_message_.select_source().screen().id());
    }
    else if (incoming_message_.has_capture_frame())
    {
        screen_capturer_->captureFrame();
    }
    else if (incoming_message_.has_capture_cursor())
    {
        // TODO
    }
    else if (incoming_message_.has_key_event())
    {
        input_injector_->injectKeyEvent(incoming_message_.key_event());
    }
    else if (incoming_message_.has_pointer_event())
    {
        input_injector_->injectPointerEvent(incoming_message_.pointer_event());
    }
    else if (incoming_message_.has_clipboard_event())
    {
        // TODO
    }
    else if (incoming_message_.has_set_desktop_features())
    {
        const proto::internal::SetDesktopFeatures& set_desktop_features =
            incoming_message_.set_desktop_features();

        screen_capturer_->enableWallpaper(set_desktop_features.wallpaper());
        screen_capturer_->enableEffects(set_desktop_features.effects());
    }
    else if (incoming_message_.has_set_block_input())
    {
        input_injector_->setBlockInput(incoming_message_.set_block_input().state());
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
    outgoing_message_.mutable_create_shared_buffer()->set_shared_buffer_id(id);
    channel_->send(common::serializeMessage(outgoing_message_));
}

void DesktopSessionAgent::onSharedMemoryDestroy(int id)
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_release_shared_buffer()->set_shared_buffer_id(id);
    channel_->send(common::serializeMessage(outgoing_message_));
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
    }

    channel_->send(common::serializeMessage(outgoing_message_));
}

void DesktopSessionAgent::onScreenCaptured(std::unique_ptr<desktop::SharedFrame> frame)
{
    outgoing_message_.Clear();

    proto::internal::SerializedDesktopFrame* serialized_frame =
        outgoing_message_.mutable_capture_frame_result()->mutable_frame();

    serialized_frame->set_shared_buffer_id(frame->sharedMemory()->id());

    desktop::Rect frame_rect = desktop::Rect::makeXYWH(frame->topLeft(), frame->size());

    codec::serializeRect(frame_rect, serialized_frame->mutable_desktop_rect());
    codec::serializePixelFormat(frame->format(), serialized_frame->mutable_pixel_format());

    for (desktop::Region::Iterator it(frame->constUpdatedRegion()); !it.isAtEnd(); it.advance())
        codec::serializeRect(it.rect(), serialized_frame->add_dirty_rect());

    last_frame_ = std::move(frame);
    channel_->send(common::serializeMessage(outgoing_message_));
}

} // namespace host
