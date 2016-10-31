/*
* PROJECT:         Aspia Remote Desktop
* FILE:            video_test/screen_sender.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_SCREEN_SENDER_H
#define _ASPIA_SCREEN_SENDER_H

#include "desktop_capture/capture_scheduler.h"
#include "desktop_capture/capturer_gdi.h"
#include "desktop_capture/desktop_region_win.h"
#include "codec/video_encoder_vp8.h"
#include "codec/video_encoder_raw.h"
#include "codec/video_encoder_zlib.h"
#include "base/thread.h"
#include "video_test/screen_reciever.h"

class ScreenSender : public Thread
{
public:
    ScreenSender(ScreenReciever *reciever);
    virtual ~ScreenSender();

    virtual void Worker() override;
    virtual void OnStart() override;
    virtual void OnStop() override;

private:
    ScreenReciever *reciever_;

    std::unique_ptr<CaptureScheduler> scheduler_;
    std::unique_ptr<CapturerGDI> capturer_;
    std::unique_ptr<VideoEncoder> encoder_;
};

#endif // _ASPIA_SCREEN_SENDER_H
