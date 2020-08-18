//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__DESKTOP__SCREEN_CAPTURER_H
#define BASE__DESKTOP__SCREEN_CAPTURER_H

#include "base/desktop/frame.h"

#include <memory>
#include <string>
#include <vector>

namespace base {

class SharedFrame;
class SharedMemoryFactory;

class ScreenCapturer
{
public:
    virtual ~ScreenCapturer() = default;

    enum class Error { SUCCEEDED, PERMANENT, TEMPORARY };

    using ScreenId = intptr_t;

    struct Screen
    {
        ScreenId id;
        std::string title;
        bool is_primary;
    };

    using ScreenList = std::vector<Screen>;

    static const ScreenId kFullDesktopScreenId = -1;
    static const ScreenId kInvalidScreenId = -2;

    virtual int screenCount() = 0;
    virtual bool screenList(ScreenList* screens) = 0;
    virtual bool selectScreen(ScreenId screen_id) = 0;
    virtual const Frame* captureFrame(Error* error) = 0;

    void setSharedMemoryFactory(SharedMemoryFactory* shared_memory_factory);
    SharedMemoryFactory* sharedMemoryFactory() const;

protected:
    friend class ScreenCapturerWrapper;
    virtual void reset() = 0;

    template <typename FrameType>
    class FrameQueue
    {
    public:
        FrameQueue() = default;

        // Moves to the next frame in the queue, moving the 'current' frame to become the
        // 'previous' one.
        void moveToNextFrame();

        void replaceCurrentFrame(std::unique_ptr<FrameType> frame);
        void reset();

        FrameType* currentFrame() const;
        FrameType* previousFrame() const;

    private:
        // Index of the current frame.
        int current_ = 0;

        static const int kQueueLength = 2;
        std::unique_ptr<FrameType> frames_[kQueueLength];

        DISALLOW_COPY_AND_ASSIGN(FrameQueue);
    };

private:
    SharedMemoryFactory* shared_memory_factory_ = nullptr;
};

template <typename FrameType>
void ScreenCapturer::FrameQueue<FrameType>::moveToNextFrame()
{
    current_ = (current_ + 1) % kQueueLength;
}

template <typename FrameType>
void ScreenCapturer::FrameQueue<FrameType>::replaceCurrentFrame(std::unique_ptr<FrameType> frame)
{
    frames_[current_] = std::move(frame);
}

template <typename FrameType>
void ScreenCapturer::FrameQueue<FrameType>::reset()
{
    for (int i = 0; i < kQueueLength; ++i)
        frames_[i].reset();
    current_ = 0;
}

template <typename FrameType>
FrameType* ScreenCapturer::FrameQueue<FrameType>::currentFrame() const
{
    return frames_[current_].get();
}

template <typename FrameType>
FrameType* ScreenCapturer::FrameQueue<FrameType>::previousFrame() const
{
    return frames_[(current_ + kQueueLength - 1) % kQueueLength].get();
}

} // namespace base

#endif // BASE__DESKTOP__CAPTURER_H
