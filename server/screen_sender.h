/*
* PROJECT:         Aspia Remote Desktop
* FILE:            server/screen_sender.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_SERVER__SCREEN_SENDER_H
#define _ASPIA_SERVER__SCREEN_SENDER_H

#include <functional>

#include "base/thread.h"
#include "base/mutex.h"
#include "base/macros.h"
#include "desktop_capture/capture_scheduler.h"
#include "desktop_capture/capturer_gdi.h"
#include "codec/video_encoder_raw.h"
#include "codec/video_encoder_vp8.h"
#include "codec/video_encoder_zlib.h"
#include "network/socket_tcp.h"

class ScreenSender : public Thread
{
public:
    typedef std::function<void(const proto::ServerToClient *message)> OnMessageAvailabeCallback;

    ScreenSender(int32_t encoding,
                 const PixelFormat &client_pixel_format,
                 OnMessageAvailabeCallback on_message_available);

    ~ScreenSender();

    void Configure(int32_t encoding, const PixelFormat &client_pixel_format);

private:
    void Worker() override;
    void OnStart() override;
    void OnStop() override;

private:
    OnMessageAvailabeCallback on_message_available_;

    std::unique_ptr<VideoEncoder> encoder_;

    Mutex update_lock_;

    PixelFormat client_pixel_format_;
    int32_t current_encoding_;

    DISALLOW_COPY_AND_ASSIGN(ScreenSender);
};

#endif // _ASPIA_SERVER__SCREEN_SENDER_H
