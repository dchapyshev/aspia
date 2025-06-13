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

#include "host/desktop_session_ipc.h"

#include "base/logging.h"
#include "base/desktop/mouse_cursor.h"

namespace host {

//--------------------------------------------------------------------------------------------------
DesktopSessionIpc::DesktopSessionIpc(base::IpcChannel* ipc_channel, QObject* parent)
    : DesktopSession(parent),
      ipc_channel_(ipc_channel)
{
    DCHECK(ipc_channel_);

    ipc_channel_->setParent(this);
    session_id_ = ipc_channel_->peerSessionId();

    LOG(LS_INFO) << "Ctor (sid=" << session_id_ << ")";
}

//--------------------------------------------------------------------------------------------------
DesktopSessionIpc::~DesktopSessionIpc()
{
    LOG(LS_INFO) << "Dtor (sid=" << session_id_ << ")";
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::start()
{
    LOG(LS_INFO) << "Start IPC desktop session (sid=" << session_id_ << ")";

    connect(ipc_channel_, &base::IpcChannel::sig_disconnected,
            this, &DesktopSessionIpc::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived,
            this, &DesktopSessionIpc::onIpcMessageReceived);

    ipc_channel_->resume();
    emit sig_desktopSessionStarted();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::control(proto::internal::DesktopControl::Action action)
{
    LOG(LS_INFO) << "Send CONTROL with action:" << action << "(sid=" << session_id_ << ")";

    outgoing_message_.newMessage().mutable_control()->set_action(action);
    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::configure(const Config& config)
{
    LOG(LS_INFO) << "Send CONFIGURE (sid=" << session_id_ << ")";

    proto::internal::Configure* configure = outgoing_message_.newMessage().mutable_configure();
    configure->set_disable_font_smoothing(config.disable_font_smoothing);
    configure->set_disable_wallpaper(config.disable_wallpaper);
    configure->set_disable_effects(config.disable_effects);
    configure->set_block_input(config.block_input);
    configure->set_lock_at_disconnect(config.lock_at_disconnect);
    configure->set_clear_clipboard(config.clear_clipboard);
    configure->set_cursor_position(config.cursor_position);

    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::selectScreen(const proto::desktop::Screen& screen)
{
    LOG(LS_INFO) << "Send SELECT_SCREEN (sid=" << session_id_ << ")";

    outgoing_message_.newMessage().mutable_select_source()->mutable_screen()->CopyFrom(screen);
    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::captureScreen()
{
    if (last_frame_.isAttached())
    {
        last_frame_.updatedRegion()->addRect(base::Rect::makeSize(last_frame_.size()));

        if (last_screen_list_)
        {
            LOG(LS_INFO) << "Has last screen list (sid=" << session_id_ << ")";
            emit sig_screenListChanged(*last_screen_list_);
        }
        else
        {
            LOG(LS_INFO) << "No last screen list (sid=" << session_id_ << ")";
        }

        base::MouseCursor* last_mouse_cursor =
            last_mouse_cursor_.isValid() ? &last_mouse_cursor_ : nullptr;

        emit sig_screenCaptured(&last_frame_, last_mouse_cursor);
    }
    else
    {
        outgoing_message_.newMessage().mutable_next_screen_capture()->set_update_interval(0);
        ipc_channel_->send(outgoing_message_.serialize());
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::setScreenCaptureFps(int fps)
{
    if (fps > 60 || fps < 1)
    {
        LOG(LS_ERROR) << "Invalid fps:" << fps << "(sid=" << session_id_ << ")";
        return;
    }

    update_interval_ = std::chrono::milliseconds(1000 / fps);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::injectKeyEvent(const proto::desktop::KeyEvent& event)
{
    outgoing_message_.newMessage().mutable_key_event()->CopyFrom(event);
    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::injectTextEvent(const proto::desktop::TextEvent& event)
{
    outgoing_message_.newMessage().mutable_text_event()->CopyFrom(event);
    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::injectMouseEvent(const proto::desktop::MouseEvent& event)
{
    outgoing_message_.newMessage().mutable_mouse_event()->CopyFrom(event);
    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::injectTouchEvent(const proto::desktop::TouchEvent &event)
{
    outgoing_message_.newMessage().mutable_touch_event()->CopyFrom(event);
    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::injectClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    outgoing_message_.newMessage().mutable_clipboard_event()->CopyFrom(event);
    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::onIpcDisconnected()
{
    LOG(LS_INFO) << "IPC channel disconnected (sid=" << session_id_ << ")";
    emit sig_desktopSessionStopped();
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::onIpcMessageReceived(const QByteArray& buffer)
{
    if (!incoming_message_.parse(buffer))
    {
        LOG(LS_ERROR) << "Invalid message from desktop (sid=" << session_id_ << ")";
        return;
    }

    if (incoming_message_->has_screen_captured())
    {
        onScreenCaptured(incoming_message_->screen_captured());
    }
    else if (incoming_message_->has_audio_packet())
    {
        emit sig_audioCaptured(incoming_message_->audio_packet());
    }
    else if (incoming_message_->has_cursor_position())
    {
        emit sig_cursorPositionChanged(incoming_message_->cursor_position());
    }
    else if (incoming_message_->has_screen_list())
    {
        LOG(LS_INFO) << "Screen list received (sid=" << session_id_ << ")";
        last_screen_list_.reset(incoming_message_->release_screen_list());
        emit sig_screenListChanged(*last_screen_list_);
    }
    else if (incoming_message_->has_screen_type())
    {
        LOG(LS_INFO) << "Screen type received (sid=" << session_id_ << ")";
        emit sig_screenTypeChanged(incoming_message_->screen_type());
    }
    else if (incoming_message_->has_shared_buffer())
    {
        switch (incoming_message_->shared_buffer().type())
        {
            case proto::internal::SharedBuffer::CREATE:
                onCreateSharedBuffer(incoming_message_->shared_buffer().shared_buffer_id());
                break;

            case proto::internal::SharedBuffer::RELEASE:
                onReleaseSharedBuffer(incoming_message_->shared_buffer().shared_buffer_id());
                break;

            default:
                NOTREACHED();
                break;
        }
    }
    else if (incoming_message_->has_clipboard_event())
    {
        emit sig_clipboardEvent(incoming_message_->clipboard_event());
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled message from desktop (sid=" << session_id_ << ")";
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::onScreenCaptured(const proto::internal::ScreenCaptured& screen_captured)
{
    const base::Frame* frame = nullptr;
    const base::MouseCursor* mouse_cursor = nullptr;

    if (screen_captured.has_frame())
    {
        const proto::internal::DesktopFrame& serialized_frame = screen_captured.frame();

        base::SharedPointer<base::SharedMemory> shared_buffer = sharedBuffer(
            serialized_frame.shared_buffer_id());
        if (shared_buffer)
        {
            last_frame_.attach(
                base::Size(serialized_frame.width(), serialized_frame.height()),
                base::PixelFormat::ARGB(),
                std::move(shared_buffer));

            last_frame_.setCapturerType(serialized_frame.capturer_type());

            base::Region* updated_region = last_frame_.updatedRegion();

            for (int i = 0; i < serialized_frame.dirty_rect_size(); ++i)
            {
                const proto::desktop::Rect& dirty_rect = serialized_frame.dirty_rect(i);
                updated_region->addRect(base::Rect::makeXYWH(
                    dirty_rect.x(), dirty_rect.y(), dirty_rect.width(), dirty_rect.height()));
            }

            frame = &last_frame_;
        }
    }

    if (screen_captured.has_mouse_cursor())
    {
        const proto::internal::MouseCursor& serialized_mouse_cursor =
            screen_captured.mouse_cursor();

        base::Size size =
            base::Size(serialized_mouse_cursor.width(), serialized_mouse_cursor.height());
        base::Point hotspot =
            base::Point(serialized_mouse_cursor.hotspot_x(), serialized_mouse_cursor.hotspot_y());
        base::Point dpi =
            base::Point(serialized_mouse_cursor.dpi_x(), serialized_mouse_cursor.dpi_y());

        last_mouse_cursor_ = base::MouseCursor(
            QByteArray::fromStdString(serialized_mouse_cursor.data()), size, hotspot, dpi);

        mouse_cursor = &last_mouse_cursor_;
    }

    if (screen_captured.error_code() == proto::desktop::VIDEO_ERROR_CODE_OK)
        emit sig_screenCaptured(frame, mouse_cursor);
    else
        emit sig_screenCaptureError(screen_captured.error_code());

    outgoing_message_.newMessage().mutable_next_screen_capture()->set_update_interval(
        update_interval_.count());
    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::onCreateSharedBuffer(int shared_buffer_id)
{
    LOG(LS_INFO) << "Shared memory created:" << shared_buffer_id << "(sid=" << session_id_ << ")";

    base::SharedMemory* shared_memory =
        base::SharedMemory::open(base::SharedMemory::Mode::READ_ONLY, shared_buffer_id);

    if (!shared_memory)
    {
        LOG(LS_ERROR) << "Failed to create the shared buffer" << shared_buffer_id
                      << "(sid=" << session_id_ << ")";
        return;
    }

    shared_buffers_[shared_buffer_id] = base::SharedPointer<base::SharedMemory>(shared_memory);
}

//--------------------------------------------------------------------------------------------------
void DesktopSessionIpc::onReleaseSharedBuffer(int shared_buffer_id)
{
    LOG(LS_INFO) << "Shared memory destroyed:" << shared_buffer_id << "(sid=" << session_id_ << ")";

    shared_buffers_.remove(shared_buffer_id);

    if (shared_buffers_.isEmpty())
    {
        LOG(LS_INFO) << "Reset last frame (sid=" << session_id_ << ")";
        last_frame_.dettach();
    }
}

//--------------------------------------------------------------------------------------------------
base::SharedPointer<base::SharedMemory> DesktopSessionIpc::sharedBuffer(int shared_buffer_id)
{
    auto result = shared_buffers_.find(shared_buffer_id);
    if (result == shared_buffers_.end())
    {
        LOG(LS_ERROR) << "Failed to find the shared buffer" << shared_buffer_id
                      << "(sid=" << session_id_ << ")";
        return nullptr;
    }

    return result.value();
}

} // namespace host
