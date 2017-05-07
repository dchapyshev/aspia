//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_desktop.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_H
#define _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_H

#include "codec/video_decoder.h"
#include "codec/cursor_decoder.h"
#include "client/client_session.h"
#include "desktop_capture/desktop_frame.h"
#include "protocol/clipboard.h"
#include "ui/viewer_window.h"

namespace aspia {

class ClientSessionDesktop :
    public ClientSession,
    private ViewerWindow::Delegate
{
public:
    ClientSessionDesktop(const ClientConfig& config, ClientSession::Delegate* delegate);
    ~ClientSessionDesktop();

private:
    // ViewerWindow::Delegate implementation.
    void OnWindowClose() override;
    void OnConfigChange(const proto::DesktopConfig& config) override;
    void OnKeyEvent(uint32_t keycode, uint32_t flags) override;
    void OnPointerEvent(int x, int y, uint32_t mask) override;
    void OnPowerEvent(proto::PowerEvent::Action action) override;
    void OnClipboardEvent(std::unique_ptr<proto::ClipboardEvent> clipboard_event) override;

    void Send(const IOBuffer* buffer) override;

    void WriteMessage(const proto::desktop::ClientToHost& message);

    bool ReadVideoPacket(const proto::VideoPacket& video_packet);
    void ReadCursorShape(const proto::CursorShape& cursor_shape);
    void ReadClipboardEvent(std::shared_ptr<proto::ClipboardEvent> clipboard_event);
    void ReadConfigRequest(const proto::DesktopConfigRequest& config_request);

    std::unique_ptr<VideoDecoder> video_decoder_;
    std::unique_ptr<CursorDecoder> cursor_decoder_;
    std::unique_ptr<ViewerWindow> viewer_;

    proto::VideoEncoding video_encoding_;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionDesktop);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_H
