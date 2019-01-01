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

#ifndef ASPIA_BASE__WIN__SCOPED_LOCAL_H_
#define ASPIA_BASE__WIN__SCOPED_LOCAL_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "base/macros_magic.h"

namespace aspia {

template <typename T>
class ScopedLocal
{
public:
    ScopedLocal() = default;

    ScopedLocal(ScopedLocal&& other) noexcept
    {
        local_ = other.local_;
        other.local_ = nullptr;
    }

    explicit ScopedLocal(T local) : local_(local)
    {
        // Nothing
    }

    explicit ScopedLocal(SIZE_T length)
    {
        local_ = reinterpret_cast<T>(LocalAlloc(LHND, length));
    }

    ~ScopedLocal() { close(); }

    T get() { return local_; }

    void reset(T local = nullptr)
    {
        close();
        local_ = local;
    }

    T release()
    {
        T local = local_;
        local_ = nullptr;
        return local;
    }

    T* recieve()
    {
        close();
        return &local_;
    }

    bool isValid() const
    {
        return (local_ != nullptr);
    }

    ScopedLocal& operator=(ScopedLocal&& other) noexcept
    {
        close();
        local_ = other.local_;
        other.local_ = nullptr;
        return *this;
    }

    operator T() { return local_; }

private:
    void close()
    {
        if (isValid())
        {
            LocalFree(local_);
            local_ = nullptr;
        }
    }

    T local_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ScopedLocal);
};

} // namespace aspia

#endif // ASPIA_BASE__WIN__SCOPED_LOCAL_H_
