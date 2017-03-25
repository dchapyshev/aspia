//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/capture_scheduler.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H_
#define ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H_

#include "aspia_config.h"

#include <stdint.h>

#include "base/macros.h"

namespace aspia {

class CaptureScheduler
{
public:
    CaptureScheduler();
    ~CaptureScheduler() = default;

    int32_t NextCaptureDelay(int32_t max_delay);

private:
    uint64_t begin_time_;

    DISALLOW_COPY_AND_ASSIGN(CaptureScheduler);
};

} // namespace aspia

#endif // ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H_
