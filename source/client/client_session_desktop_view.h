//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_desktop_view.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_VIEW_H
#define _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_VIEW_H

#include "codec/video_decoder.h"
#include "codec/cursor_decoder.h"
#include "client/client_session.h"
#include "ui/viewer_window.h"

namespace aspia {

class ClientSessionDesktopView :
    public ClientSession,
    private ViewerWindow::Delegate
{
public:
    ClientSessionDesktopView(const ClientConfig& config,
                             std::shared_ptr<NetworkChannelProxy> channel_proxy);
    ~ClientSessionDesktopView();

protected:
    void WriteMessage(const proto::desktop::ClientToHost& message);
    void ReadConfigRequest(const proto::DesktopSessionConfigRequest& config_request);
    bool ReadVideoPacket(const proto::VideoPacket& video_packet);

    std::unique_ptr<ViewerWindow> viewer_;

private:
    // ViewerWindow::Delegate implementation.
    void OnWindowClose() override;
    void OnConfigChange(const proto::DesktopSessionConfig& config) override;
    void OnKeyEvent(uint32_t keycode, uint32_t flags) override { }
    void OnPointerEvent(const DesktopPoint& pos, uint32_t mask) override { }
    void OnClipboardEvent(proto::ClipboardEvent& clipboard_event) override { }

    virtual void OnMessageReceived(const IOBuffer& buffer);

    std::unique_ptr<VideoDecoder> video_decoder_;
    proto::VideoEncoding video_encoding_ = proto::VIDEO_ENCODING_UNKNOWN;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionDesktopView);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_VIEW_H
