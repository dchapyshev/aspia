//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/screen_sender.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__SCREEN_SENDER_H
#define _ASPIA_HOST__SCREEN_SENDER_H

#include "aspia_config.h"

#include <functional>

#include "base/thread.h"
#include "base/lock.h"
#include "base/macros.h"
#include "desktop_capture/scoped_desktop_effects.h"
#include "desktop_capture/capture_scheduler.h"
#include "desktop_capture/capturer_gdi.h"
#include "codec/video_encoder_zlib.h"
#include "codec/video_encoder_vp8.h"
#include "codec/video_encoder_vp9.h"
#include "codec/cursor_encoder.h"
#include "network/socket_tcp.h"

namespace aspia {

class ScreenSender : private Thread
{
public:
    typedef std::function<bool(const proto::desktop::HostToClient&)> MessageCallback;

    explicit ScreenSender();
    ~ScreenSender();

    bool StartSending(const proto::DesktopConfig& config,
                      const MessageCallback& message_callback);
    void StopSending();

private:
    void Worker() override;
    void OnStop() override;

private:
    MessageCallback message_callback_;

    std::unique_ptr<VideoEncoder> video_encoder_;
    std::unique_ptr<CursorEncoder> cursor_encoder_;
    std::unique_ptr<Capturer> capturer_;

    proto::DesktopConfig config_;

    std::unique_ptr<ScopedDesktopEffects> desktop_effects_;

    Lock update_lock_;

    proto::desktop::HostToClient message_;

    DISALLOW_COPY_AND_ASSIGN(ScreenSender);
};

} // namespace aspia

#endif // _ASPIA_HOST__SCREEN_SENDER_H
