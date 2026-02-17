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

#ifndef BASE_WIN_SCOPED_CO_MEM_H
#define BASE_WIN_SCOPED_CO_MEM_H

#include "base/logging.h"

#include <objbase.h>

namespace base {

// Simple scoped memory releaser class for COM allocated memory.
// Example:
//   base::ScopedCoMem<ITEMIDLIST> file_item;
//   SHGetSomeInfo(&file_item, ...);
//   ...
//   return;  <-- memory released
template<typename T>
class ScopedCoMem
{
public:
    ScopedCoMem() = default;

    ~ScopedCoMem()
    {
        reset(nullptr);
    }

    T** operator&()
    {
        DCHECK(mem_ptr_ == nullptr);
        return &mem_ptr_;
    }

    operator T*() { return mem_ptr_; }

    T* operator->()
    {
        DCHECK(mem_ptr_ != nullptr);
        return mem_ptr_;
    }

    const T* operator->() const
    {
        DCHECK(mem_ptr_ != nullptr);
        return mem_ptr_;
    }

    void reset(T* ptr)
    {
        CoTaskMemFree(mem_ptr_);
        mem_ptr_ = ptr;
    }

    T* get() const { return mem_ptr_; }

private:
    T* mem_ptr_ = nullptr;

    Q_DISABLE_COPY(ScopedCoMem)
};

} // namespace base

#endif // BASE_WIN_SCOPED_CO_MEM_H
