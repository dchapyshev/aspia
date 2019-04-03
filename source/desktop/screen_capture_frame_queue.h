//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef DESKTOP__SCREEN_CAPTURE_FRAME_QUEUE_H
#define DESKTOP__SCREEN_CAPTURE_FRAME_QUEUE_H

#include "base/macros_magic.h"

#include <memory>

namespace desktop {

class Frame;

template <typename FrameType>
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

    void replaceCurrentFrame(std::unique_ptr<FrameType> frame)
    {
        frames_[current_] = std::move(frame);
    }

    void reset()
    {
        for (int i = 0; i < kQueueLength; ++i)
            frames_[i].reset();
        current_ = 0;
    }

    FrameType* currentFrame() const
    {
        return frames_[current_].get();
    }

    FrameType* previousFrame() const
    {
        return frames_[(current_ + kQueueLength - 1) % kQueueLength].get();
    }

private:
    // Index of the current frame.
    int current_ = 0;

    static const int kQueueLength = 2;
    std::unique_ptr<FrameType> frames_[kQueueLength];

    DISALLOW_COPY_AND_ASSIGN(ScreenCaptureFrameQueue);
};

} // namespace desktop

#endif // DESKTOP__SCREEN_CAPTURE_FRAME_QUEUE_H
