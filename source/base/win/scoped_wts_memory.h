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

#ifndef ASPIA_BASE__WIN__SCOPED_WTS_MEMORY_H_
#define ASPIA_BASE__WIN__SCOPED_WTS_MEMORY_H_

#include <wtsapi32.h>

#include "base/macros_magic.h"

template <typename T>
class ScopedWtsMemory
{
public:
    ScopedWtsMemory() = default;

    explicit ScopedWtsMemory(T memory)
        : memory_(memory)
    {
        // Nothing
    }

    ~ScopedWtsMemory() { close(); }

    T get() { return memory_; }

    void reset(T memory)
    {
        close();
        memory_ = memory;
    }

    T release()
    {
        T memory = memory_;
        memory_ = nullptr;
        return memory;
    }

    T* recieve()
    {
        close();
        return &memory_;
    }

    bool isValid() const
    {
        return (memory_ != nullptr);
    }

    operator T() { return memory_; }

    T operator [](DWORD index) const
    {
        return &memory_[index];
    }

private:
    void close()
    {
        if (isValid())
        {
            WTSFreeMemory(memory_);
            memory_ = nullptr;
        }
    }

    T memory_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ScopedWtsMemory);
};

#endif // ASPIA_BASE__WIN__SCOPED_WTS_MEMORY_H_
