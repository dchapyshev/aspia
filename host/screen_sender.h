/*
* PROJECT:         Aspia Remote Desktop
* FILE:            host/screen_sender.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_HOST__SCREEN_SENDER_H
#define _ASPIA_HOST__SCREEN_SENDER_H

#include <functional>

#include "base/thread.h"
#include "base/lock.h"
#include "base/macros.h"
#include "desktop_capture/capture_scheduler.h"
#include "desktop_capture/capturer_gdi.h"
#include "codec/video_encoder_vp8.h"
#include "codec/video_encoder_zlib.h"
#include "network/socket_tcp.h"

namespace aspia {

class ScreenSender : private Thread
{
public:
    typedef std::function<void(const proto::HostToClient *message)> OnMessageCallback;

    ScreenSender(int32_t encoding,
                 const PixelFormat &client_pixel_format,
                 OnMessageCallback on_message);

    ~ScreenSender();

    void Configure(int32_t encoding, const PixelFormat &client_pixel_format);

private:
    void Worker() override;
    void OnStop() override;

private:
    OnMessageCallback on_message_;

    std::unique_ptr<VideoEncoder> encoder_;

    Lock update_lock_;

    PixelFormat client_pixel_format_;
    int32_t current_encoding_;

    DISALLOW_COPY_AND_ASSIGN(ScreenSender);
};

} // namespace aspia

#endif // _ASPIA_HOST__SCREEN_SENDER_H
