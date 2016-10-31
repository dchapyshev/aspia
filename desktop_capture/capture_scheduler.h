/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/capture_scheduler.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H_
#define ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H_

#include "aspia_config.h"

#include <stdint.h>
#include <algorithm>

#include "base/macros.h"
#include "base/logging.h"

class CaptureScheduler
{
public:
    CaptureScheduler();
    ~CaptureScheduler();

    // Determine the time delay from current time to perform next capture.
    uint32_t NextCaptureDelay();

    void BeginCapture();

private:
    uint32_t begin_time_;
    int32_t diff_time_;

    DISALLOW_COPY_AND_ASSIGN(CaptureScheduler);
};

#endif // ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H_
