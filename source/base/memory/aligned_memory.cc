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

#include "base/memory/aligned_memory.h"

#include "base/logging.h"

#if defined(Q_OS_ANDROID)
#include <malloc.h>
#endif

namespace base {

//--------------------------------------------------------------------------------------------------
void* alignedAlloc(size_t size, size_t alignment)
{
    DCHECK_GT(size, 0U);
    DCHECK_EQ((alignment & (alignment - 1)), 0U);
    DCHECK_EQ((alignment % sizeof(void*)), 0U);

    void* ptr = nullptr;

#if defined(Q_OS_WINDOWS)
    ptr = _aligned_malloc(size, alignment);
#elif defined(Q_OS_ANDROID)
    ptr = memalign(alignment, size);
#else
    if (posix_memalign(&ptr, alignment, size))
        ptr = nullptr;
#endif

    CHECK(ptr);

    // Sanity check alignment just to be safe.
    DCHECK_EQ((reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)), 0U);
    return ptr;
}

} // namespace base
