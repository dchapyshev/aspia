/*
* PROJECT:         Aspia Remote Desktop
* FILE:            video_test/screen_reciever.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_SCREEN_RECIEVER_H
#define _ASPIA_SCREEN_RECIEVER_H

#include "desktop_capture/screen_window_win.h"
#include "desktop_capture/desktop_region_win.h"
#include "codec/video_decoder_vp8.h"
#include "codec/video_decoder_raw.h"
#include "codec/video_decoder_zlib.h"
#include "base/thread.h"
#include "base/mutex.h"
#include "base/event.h"

#include <queue>

class ScreenReciever : public Thread
{
public:
    ScreenReciever();
    virtual ~ScreenReciever() override;

    void ReadPacket(proto::VideoPacket *packet);

    virtual void Worker() override;
    virtual void OnStop() override;
    virtual void OnStart() override;

private:
    std::unique_ptr<ScreenWindowWin> window_;

    Event packet_event_;

    Mutex packet_queue_lock_;
    std::queue<std::string> packet_queue_;

    proto::VideoEncoding current_encoding_;

    google::protobuf::Arena arena_;

    std::unique_ptr<VideoDecoder> decoder_;
};

#endif // _ASPIA_SCREEN_RECIEVER_H
