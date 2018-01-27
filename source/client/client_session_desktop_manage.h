//
// PROJECT:         Aspia
// FILE:            client/client_session_desktop_manage.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_MANAGE_H
#define _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_MANAGE_H

#include "client/client_session_desktop_view.h"
#include "codec/cursor_decoder.h"

namespace aspia {

class ClientSessionDesktopManage : public ClientSessionDesktopView
{
public:
    ClientSessionDesktopManage(const proto::Computer& computer,
                               std::shared_ptr<NetworkChannelProxy> channel_proxy);
    ~ClientSessionDesktopManage();

private:
    // ViewerWindow::Delegate implementation.
    void OnKeyEvent(uint32_t keycode, uint32_t flags) override;
    void OnPointerEvent(const DesktopPoint& pos, uint32_t mask) override;
    void OnClipboardEvent(proto::desktop::ClipboardEvent& clipboard_event) override;

    void OnMessageReceived(const IOBuffer& buffer) override;

    void ReadCursorShape(const proto::desktop::CursorShape& cursor_shape);
    void ReadClipboardEvent(std::shared_ptr<proto::desktop::ClipboardEvent> clipboard_event);

    std::unique_ptr<CursorDecoder> cursor_decoder_;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionDesktopManage);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_H
