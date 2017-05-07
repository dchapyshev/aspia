//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/screen_updater.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__SCREEN_UPDATER_H
#define _ASPIA_HOST__SCREEN_UPDATER_H

#include "base/thread.h"
#include "base/macros.h"
#include "desktop_capture/capture_scheduler.h"
#include "desktop_capture/capturer_gdi.h"

#include <chrono>

namespace aspia {

class ScreenUpdater : private Thread
{
public:
    ScreenUpdater();
    ~ScreenUpdater();

    class Delegate
    {
    public:
        virtual void OnScreenUpdate(const DesktopFrame* screen_frame) = 0;
        virtual void OnCursorUpdate(std::unique_ptr<MouseCursor> mouse_cursor) = 0;
        virtual void OnScreenUpdateError() = 0;
    };

    enum class Mode { SCREEN_AND_CURSOR, SCREEN_ONLY };

    bool StartUpdating(Mode mode,
                       const std::chrono::milliseconds& interval,
                       Delegate* delegate);

private:
    void Run() override;

    Delegate* delegate_;
    Mode mode_;
    std::chrono::milliseconds interval_;

    DISALLOW_COPY_AND_ASSIGN(ScreenUpdater);
};

} // namespace aspia

#endif // _ASPIA_HOST__SCREEN_UPDATER_H
