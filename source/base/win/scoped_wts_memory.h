//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_WIN_SCOPED_WTS_MEMORY_H
#define BASE_WIN_SCOPED_WTS_MEMORY_H

#include "base/macros_magic.h"

#include <qt_windows.h>
#include <WtsApi32.h>

namespace base {

template <typename T>
class ScopedWtsMemory
{
public:
    ScopedWtsMemory() = default;

    explicit ScopedWtsMemory(T* memory)
        : memory_(memory)
    {
        // Nothing
    }

    ~ScopedWtsMemory() { close(); }

    T* get() { return memory_; }

    void reset(T* memory)
    {
        close();
        memory_ = memory;
    }

    T* release()
    {
        T* memory = memory_;
        memory_ = nullptr;
        return memory;
    }

    T** recieve()
    {
        close();
        return &memory_;
    }

    bool isValid() const
    {
        return (memory_ != nullptr);
    }

    T* operator [](DWORD index) const
    {
        return &memory_[index];
    }

    T* operator->() const { return memory_; }

private:
    void close()
    {
        if (isValid())
        {
            WTSFreeMemory(memory_);
            memory_ = nullptr;
        }
    }

    T* memory_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ScopedWtsMemory);
};

} // namespace base

#endif // BASE_WIN_SCOPED_WTS_MEMORY_H
