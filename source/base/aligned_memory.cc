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

#include "base/aligned_memory.h"

namespace aspia {

void* alignedAlloc(size_t size, size_t alignment)
{
    Q_ASSERT(size > 0U);
    Q_ASSERT((alignment & (alignment - 1)) == 0U);
    Q_ASSERT((alignment % sizeof(void*)) == 0U);

    void* ptr =  qMallocAligned(size, alignment);
    Q_CHECK_PTR(ptr);

    // Sanity check alignment just to be safe.
    Q_ASSERT((reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0U);

    return ptr;
}

}  // namespace aspia
