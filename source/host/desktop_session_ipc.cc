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

#include "host/desktop_session_ipc.h"

#include "base/logging.h"
#include "codec/video_util.h"
#include "common/message_serialization.h"
#include "desktop/shared_memory_desktop_frame.h"
#include "ipc/ipc_channel.h"
#include "ipc/shared_memory.h"

namespace host {

class DesktopSessionIpc::SharedBuffer : public ipc::SharedMemoryBase
{
public:
    ~SharedBuffer() = default;

    static std::unique_ptr<SharedBuffer> wrap(std::unique_ptr<ipc::SharedMemory> shared_memory)
    {
        std::shared_ptr<ipc::SharedMemory> shared_frame(shared_memory.release());
        return std::unique_ptr<SharedBuffer>(new SharedBuffer(shared_frame));
    }

    std::unique_ptr<SharedBuffer> share()
    {
        return std::unique_ptr<SharedBuffer>(new SharedBuffer(shared_memory_));
    }

    void* data() override
    {
        return shared_memory_->data();
    }

    Handle handle() const override
    {
        return shared_memory_->handle();
    }

    int id() const override
    {
        return shared_memory_->id();
    }

private:
    explicit SharedBuffer(std::shared_ptr<ipc::SharedMemory>& shared_memory)
        : shared_memory_(shared_memory)
    {
        // Nothing
    }

    std::shared_ptr<ipc::SharedMemory> shared_memory_;

    DISALLOW_COPY_AND_ASSIGN(SharedBuffer);
};

DesktopSessionIpc::DesktopSessionIpc(std::unique_ptr<ipc::Channel> channel, Delegate* delegate)
    : channel_(std::move(channel)),
      delegate_(delegate)
{
    DCHECK(channel_);
    DCHECK(delegate_);
}

DesktopSessionIpc::~DesktopSessionIpc() = default;

void DesktopSessionIpc::start()
{
    channel_->resume();
    delegate_->onDesktopSessionStarted();
}

void DesktopSessionIpc::captureScreen()
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_capture_frame()->set_dummy(1);
    channel_->send(common::serializeMessage(outgoing_message_));
}

void DesktopSessionIpc::selectScreen(const proto::Screen& screen)
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_select_source()->mutable_screen()->CopyFrom(screen);
    channel_->send(common::serializeMessage(outgoing_message_));
}

void DesktopSessionIpc::injectKeyEvent(const proto::KeyEvent& event)
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_key_event()->CopyFrom(event);
    channel_->send(common::serializeMessage(outgoing_message_));
}

void DesktopSessionIpc::injectPointerEvent(const proto::PointerEvent& event)
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_pointer_event()->CopyFrom(event);
    channel_->send(common::serializeMessage(outgoing_message_));
}

void DesktopSessionIpc::injectClipboardEvent(const proto::ClipboardEvent& event)
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_clipboard_event()->CopyFrom(event);
    channel_->send(common::serializeMessage(outgoing_message_));
}

void DesktopSessionIpc::onDisconnected()
{
    delegate_->onDesktopSessionStopped();
}

void DesktopSessionIpc::onMessageReceived(const base::ByteArray& buffer)
{
    incoming_message_.Clear();

    if (!common::parseMessage(buffer, &incoming_message_))
    {
        LOG(LS_ERROR) << "Invalid message from desktop";
        return;
    }

    if (incoming_message_.has_screen_list())
    {
        delegate_->onScreenListChanged(incoming_message_.screen_list());
    }
    else if (incoming_message_.has_create_shared_buffer())
    {
        onCreateSharedBuffer(incoming_message_.create_shared_buffer().shared_buffer_id());
    }
    else if (incoming_message_.has_release_shared_buffer())
    {
        onReleaseSharedBuffer(incoming_message_.release_shared_buffer().shared_buffer_id());
    }
    else if (incoming_message_.has_capture_frame_result())
    {
        onCaptureFrameResult(incoming_message_.capture_frame_result());
    }
    else if (incoming_message_.has_capture_cursor_result())
    {
        onCaptureCursorResult(incoming_message_.capture_cursor_result());
    }
    else if (incoming_message_.has_clipboard_event())
    {
        delegate_->onClipboardEvent(incoming_message_.clipboard_event());
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled message from desktop";
        return;
    }
}

void DesktopSessionIpc::onCaptureFrameResult(const proto::internal::CaptureFrameResult& result)
{
    const proto::internal::SerializedDesktopFrame& serialized_frame = result.frame();

    std::unique_ptr<SharedBuffer> shared_buffer = sharedBuffer(serialized_frame.shared_buffer_id());
    if (!shared_buffer)
        return;

    const proto::Rect& frame_rect = serialized_frame.desktop_rect();

    std::unique_ptr<desktop::Frame> frame =
        desktop::SharedMemoryFrame::attach(desktop::Size(frame_rect.width(), frame_rect.height()),
                                           codec::parsePixelFormat(serialized_frame.pixel_format()),
                                           std::move(shared_buffer));

    frame->setTopLeft(desktop::Point(frame_rect.x(), frame_rect.y()));

    for (int i = 0; i < serialized_frame.dirty_rect_size(); ++i)
        frame->updatedRegion()->addRect(codec::parseRect(serialized_frame.dirty_rect(i)));

    delegate_->onScreenCaptured(std::move(frame));
}

void DesktopSessionIpc::onCaptureCursorResult(const proto::internal::CaptureCursorResult& result)
{
    // TODO
}

void DesktopSessionIpc::onCreateSharedBuffer(int shared_buffer_id)
{
    std::unique_ptr<ipc::SharedMemory> shared_memory =
        ipc::SharedMemory::open(ipc::SharedMemory::Mode::READ_ONLY, shared_buffer_id);

    if (!shared_memory)
    {
        LOG(LS_ERROR) << "Failed to create the shared buffer " << shared_buffer_id;
        return;
    }

    shared_buffers_.emplace(shared_buffer_id, SharedBuffer::wrap(std::move(shared_memory)));
}

void DesktopSessionIpc::onReleaseSharedBuffer(int shared_buffer_id)
{
    shared_buffers_.erase(shared_buffer_id);
}

std::unique_ptr<DesktopSessionIpc::SharedBuffer> DesktopSessionIpc::sharedBuffer(
    int shared_buffer_id)
{
    auto result = shared_buffers_.find(shared_buffer_id);
    if (result == shared_buffers_.end())
    {
        LOG(LS_ERROR) << "Failed to find the shared buffer " << shared_buffer_id;
        return nullptr;
    }

    return result->second->share();
}

} // namespace host
