//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_DESKTOP_SHARED_FRAME_H
#define BASE_DESKTOP_SHARED_FRAME_H

#include <QSize>

#include <memory>
#include <mutex>

class Frame;

// A copyable, atomically reference-counted handle to a Frame. Copies share the same underlying frame
// and lock, so the handle can be passed by value across threads (for example through a queued
// signal). The pixel buffer is mutable shared state - the decode/convert thread writes it while the
// GUI thread reads it - so the only way to reach the Frame is through the read()/write() access
// objects, which hold the lock for their lifetime; the buffer can never be touched without the lock.
// The frame size is immutable after construction and has no writer, so it is exposed directly through
// size() without locking.
class SharedFrame
{
public:
    SharedFrame() = default;
    explicit SharedFrame(std::unique_ptr<Frame> frame);

    explicit operator bool() const { return core_ != nullptr; }

    // Immutable metadata; safe to read without a lock.
    QSize size() const;

    // Scoped read access to the pixel buffer; holds the lock while alive.
    class ReadAccess
    {
    public:
        const Frame& get() const { return frame_; }

    private:
        friend class SharedFrame;
        ReadAccess(std::mutex& mutex, const Frame& frame)
            : lock_(mutex), frame_(frame) {}

        std::unique_lock<std::mutex> lock_;
        const Frame& frame_;
    };

    // Scoped write access to the pixel buffer; holds the lock while alive.
    class WriteAccess
    {
    public:
        Frame& get() const { return frame_; }

    private:
        friend class SharedFrame;
        WriteAccess(std::mutex& mutex, Frame& frame)
            : lock_(mutex), frame_(frame) {}

        std::unique_lock<std::mutex> lock_;
        Frame& frame_;
    };

    [[nodiscard]] ReadAccess read() const;
    [[nodiscard]] WriteAccess write();

private:
    struct Core;
    std::shared_ptr<Core> core_;
};

#endif // BASE_DESKTOP_SHARED_FRAME_H
