/*
* PROJECT:         Aspia Remote Desktop
* FILE:            capturer_test/screen_updater.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_SCREEN_UPDATER_H
#define _ASPIA_SCREEN_UPDATER_H

#include "desktop_capture/capture_scheduler.h"
#include "desktop_capture/capturer_gdi.h"
#include "desktop_capture/desktop_region_win.h"
#include "desktop_capture/screen_window_win.h"

#include "base/thread.h"

class ScreenUpdater : public Thread
{
public:
    ScreenUpdater();
    virtual ~ScreenUpdater();

    virtual void Worker() override;
    void OnStart() override;
    void OnStop() override;

private:
    std::unique_ptr<ScreenWindowWin> window_;

    std::unique_ptr<CaptureScheduler> scheduler_;
    std::unique_ptr<CapturerGDI> capturer_;
};

#endif // _ASPIA_SCREEN_UPDATER_H
