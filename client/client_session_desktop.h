//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_desktop.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_H
#define _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_H

#include "aspia_config.h"

#include "base/macros.h"
#include "codec/video_decoder_zlib.h"
#include "codec/video_decoder_vp8.h"
#include "codec/video_decoder_vp9.h"
#include "codec/cursor_decoder.h"
#include "desktop_capture/desktop_frame.h"
#include "client/client_session.h"
#include "client/client_config.h"
#include "protocol/message_queue.h"
#include "protocol/clipboard.h"
#include "proto/desktop_session.pb.h"

namespace aspia {

class ClientSessionDesktop : public ClientSession
{
public:
    explicit ClientSessionDesktop(std::unique_ptr<Socket> sock);
    ~ClientSessionDesktop() = default;

    virtual DesktopFrame* GetVideoFrame() = 0;
    virtual void VideoFrameUpdated() = 0;
    virtual void ResizeVideoFrame(const DesktopSize& size, const PixelFormat& format) = 0;

    virtual void OnCursorUpdate(const MouseCursor* mouse_cursor) = 0;
    virtual void OnConfigRequest() = 0;

    void SendPointerEvent(int32_t x, int32_t y, uint32_t mask);
    void SendKeyEvent(uint32_t keycode, uint32_t flags = 0);
    void SendPowerEvent(proto::PowerEvent::Action action);

    void ConfigureSession(const ScreenConfig& config);

private:
    bool OnIncommingMessage(const uint8_t* buffer, uint32_t size) override;

    bool WriteMessage(const proto::desktop::ClientToHost* message);
    void SendClipboardEvent(const proto::ClipboardEvent& event);

    bool ProcessIncommingMessage(const proto::desktop::HostToClient* message);
    bool ReadVideoPacket(const proto::VideoPacket& packet);
    bool ReadCursorShape(const proto::CursorShape& cursor_shape);
    bool ReadClipboardEvent(const proto::ClipboardEvent& event);
    bool ReadConfigRequest(const proto::DesktopConfigRequest& config_request);

private:
    std::unique_ptr<MessageQueue<proto::desktop::HostToClient>> incomming_queue_;
    std::unique_ptr<MessageQueue<proto::desktop::ClientToHost>> outgoing_queue_;

    ScopedAlignedBuffer outgoing_buffer_;

    std::unique_ptr<VideoDecoder> video_decoder_;
    std::unique_ptr<CursorDecoder> cursor_decoder_;

    Clipboard clipboard_;

    proto::VideoEncoding video_encoding_;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionDesktop);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_H
