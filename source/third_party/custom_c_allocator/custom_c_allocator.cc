//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "third_party/custom_c_allocator/custom_c_allocator.h"

#include <cstdlib>

#if defined(USE_TBB_ALLOCATOR)
#include <tbb/scalable_allocator.h>
#endif

void* custom_c_malloc(size_t size)
{
#if defined(USE_TBB_ALLOCATOR)
    return scalable_malloc(size);
#else
    return malloc(size);
#endif
}

void custom_c_free(void* ptr)
{
#if defined(USE_TBB_ALLOCATOR)
    scalable_free(ptr);
#else
    free(ptr);
#endif
}

void* custom_c_realloc(void* ptr, size_t size)
{
#if defined(USE_TBB_ALLOCATOR)
    return scalable_realloc(ptr, size);
#else
    return realloc(ptr, size);
#endif
}
