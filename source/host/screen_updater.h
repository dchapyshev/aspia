//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/screen_updater.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__SCREEN_UPDATER_H
#define _ASPIA_HOST__SCREEN_UPDATER_H

#include "base/message_loop/message_loop_thread.h"
#include "desktop_capture/capture_scheduler.h"
#include "desktop_capture/capturer_gdi.h"

namespace aspia {

class ScreenUpdater
{
public:
    enum class Mode { SCREEN_AND_CURSOR, SCREEN };

    using ScreenUpdateCallback =
        std::function<void(const DesktopFrame* screen_frame,
                           std::unique_ptr<MouseCursor> mouse_cursor)>;

    ScreenUpdater(Mode mode,
                  const std::chrono::milliseconds& update_interval,
                  ScreenUpdateCallback screen_update_callback);
    ~ScreenUpdater();

    void PostUpdateRequest();

private:
    void UpdateScreen();

    ScreenUpdateCallback screen_update_callback_;

    MessageLoopThread thread_;
    std::shared_ptr<MessageLoopProxy> runner_;
    std::unique_ptr<Capturer> capturer_;
    CaptureScheduler scheduler_;

    const Mode mode_;
    const std::chrono::milliseconds update_interval_;

    DISALLOW_COPY_AND_ASSIGN(ScreenUpdater);
};

} // namespace aspia

#endif // _ASPIA_HOST__SCREEN_UPDATER_H
