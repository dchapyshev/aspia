//
// PROJECT:         Aspia
// FILE:            desktop_capture/capture_scheduler.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H
#define _ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H

#include <chrono>

namespace aspia {

class CaptureScheduler
{
public:
    CaptureScheduler() = default;
    ~CaptureScheduler() = default;

    void beginCapture();
    std::chrono::milliseconds nextCaptureDelay(const std::chrono::milliseconds& max_delay) const;

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> begin_time_;

    Q_DISABLE_COPY(CaptureScheduler)
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H
