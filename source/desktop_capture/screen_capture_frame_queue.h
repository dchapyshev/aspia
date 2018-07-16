//
// PROJECT:         Aspia
// FILE:            desktop_capture/screen_capture_frame_queue.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
// NOTE:            This file based on WebRTC source code
//

#ifndef ASPIA_DESKTOP_CAPTURE__SCREEN_CAPTURE_FRAME_QUEUE_H_
#define ASPIA_DESKTOP_CAPTURE__SCREEN_CAPTURE_FRAME_QUEUE_H_

#include <memory>

namespace aspia {

class DesktopFrame;

class ScreenCaptureFrameQueue
{
public:
    ScreenCaptureFrameQueue() = default;

    // Moves to the next frame in the queue, moving the 'current' frame to become the 'previous'
    // one.
    void moveToNextFrame()
    {
        current_ = (current_ + 1) % kQueueLength;
    }

    void replaceCurrentFrame(std::unique_ptr<DesktopFrame> frame)
    {
        frames_[current_] = std::move(frame);
    }

    void reset()
    {
        for (int i = 0; i < kQueueLength; ++i)
            frames_[i].reset();
        current_ = 0;
    }

    DesktopFrame* currentFrame() const
    {
        return frames_[current_].get();
    }

    DesktopFrame* previousFrame() const
    {
        return frames_[(current_ + kQueueLength - 1) % kQueueLength].get();
    }

private:
    // Index of the current frame.
    int current_ = 0;

    static const int kQueueLength = 2;
    std::unique_ptr<DesktopFrame> frames_[kQueueLength];

    Q_DISABLE_COPY(ScreenCaptureFrameQueue)
};

} // namespace aspia

#endif // ASPIA_DESKTOP_CAPTURE__SCREEN_CAPTURE_FRAME_QUEUE_H_
