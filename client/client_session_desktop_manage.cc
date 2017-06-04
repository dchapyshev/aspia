//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_desktop_manage.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_desktop_manage.h"
#include "base/logging.h"
#include "codec/video_decoder_zlib.h"
#include "codec/video_decoder_vpx.h"
#include "codec/video_helpers.h"
#include "protocol/message_serialization.h"

namespace aspia {

ClientSessionDesktopManage::ClientSessionDesktopManage(const ClientConfig& config,
                                                       ClientSession::Delegate* delegate) :
    ClientSessionDesktopView(config, delegate)
{
    // Nothing
}

ClientSessionDesktopManage::~ClientSessionDesktopManage()
{
    // Nothing
}

void ClientSessionDesktopManage::ReadCursorShape(const proto::CursorShape& cursor_shape)
{
    if (!cursor_decoder_)
        cursor_decoder_.reset(new CursorDecoder());

    std::shared_ptr<MouseCursor> mouse_cursor = cursor_decoder_->Decode(cursor_shape);

    if (mouse_cursor)
        viewer_->InjectMouseCursor(mouse_cursor);
}

void ClientSessionDesktopManage::ReadClipboardEvent(std::shared_ptr<proto::ClipboardEvent> clipboard_event)
{
    viewer_->InjectClipboardEvent(clipboard_event);
}

void ClientSessionDesktopManage::Send(const IOBuffer& buffer)
{
    proto::desktop::HostToClient message;

    if (ParseMessage(buffer, message))
    {
        bool success = true;

        if (message.has_video_packet())
        {
            success = ReadVideoPacket(message.video_packet());
        }
        else if (message.has_audio_packet())
        {
            LOG(WARNING) << "Audio not implemented yet";
        }
        else if (message.has_cursor_shape())
        {
            ReadCursorShape(message.cursor_shape());
        }
        else if (message.has_clipboard_event())
        {
            std::shared_ptr<proto::ClipboardEvent> clipboard_event(
                message.release_clipboard_event());

            ReadClipboardEvent(clipboard_event);
        }
        else if (message.has_config_request())
        {
            ReadConfigRequest(message.config_request());
        }
        else
        {
            // Unknown messages are ignored.
            DLOG(WARNING) << "Unhandled message from host";
        }

        if (success)
            return;
    }

    delegate_->OnSessionTerminate();
}

void ClientSessionDesktopManage::OnKeyEvent(uint32_t keycode, uint32_t flags)
{
    proto::desktop::ClientToHost message;

    proto::KeyEvent* event = message.mutable_key_event();
    event->set_keycode(keycode);
    event->set_flags(flags);

    WriteMessage(message);
}

void ClientSessionDesktopManage::OnPointerEvent(const DesktopPoint& pos, uint32_t mask)
{
    proto::desktop::ClientToHost message;

    proto::PointerEvent* event = message.mutable_pointer_event();
    event->set_x(pos.x());
    event->set_y(pos.y());
    event->set_mask(mask);

    WriteMessage(message);
}

void ClientSessionDesktopManage::OnPowerEvent(proto::PowerEvent::Action action)
{
    proto::desktop::ClientToHost message;
    message.mutable_power_event()->set_action(action);
    WriteMessage(message);
}

void ClientSessionDesktopManage::OnClipboardEvent(std::unique_ptr<proto::ClipboardEvent> clipboard_event)
{
    proto::desktop::ClientToHost message;
    message.set_allocated_clipboard_event(clipboard_event.release());
    WriteMessage(message);
}

} // namespace aspia
