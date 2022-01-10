//
// Aspia Project
// Copyright (C) 2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#if defined(USE_TBB_ALLOCATOR)

#include <tbb/scalable_allocator.h>

void* operator new(size_t size)
{
    void* ptr = scalable_malloc(size);
    if (!ptr)
        throw std::bad_alloc{};

    return ptr;
}

void operator delete(void* ptr) noexcept
{
    scalable_free(ptr);
}

//--------------------------------------------------------------------------------------------------

void* operator new(size_t size, std::nothrow_t const&) noexcept
{
    return scalable_malloc(size);
}

void operator delete(void* ptr, std::nothrow_t const&) noexcept
{
    scalable_free(ptr);
}

//--------------------------------------------------------------------------------------------------

void* operator new[](size_t size)
{
    void* ptr = scalable_malloc(size);
    if (!ptr)
        throw std::bad_alloc{};

    return ptr;
}

void operator delete[](void* ptr) noexcept
{
    scalable_free(ptr);
}

//--------------------------------------------------------------------------------------------------

void* operator new[](size_t size, std::nothrow_t const&) noexcept
{
    return scalable_malloc(size);
}

void operator delete[](void* ptr, std::nothrow_t const&) noexcept
{
    scalable_free(ptr);
}

//--------------------------------------------------------------------------------------------------

void* operator new(size_t size, std::align_val_t align)
{
    void* ptr = scalable_aligned_malloc(size, static_cast<size_t>(align));
    if (!ptr)
        throw std::bad_alloc{};

    return ptr;
}

void operator delete(void* ptr, std::align_val_t /* align */) noexcept
{
    scalable_aligned_free(ptr);
}

//--------------------------------------------------------------------------------------------------

void* operator new(size_t size, std::align_val_t align, std::nothrow_t const&) noexcept
{
    return scalable_aligned_malloc(size, static_cast<size_t>(align));
}

void operator delete(void* ptr, std::align_val_t /* align */, std::nothrow_t const&) noexcept
{
    scalable_aligned_free(ptr);
}

//--------------------------------------------------------------------------------------------------

void* operator new[](size_t size, std::align_val_t align)
{
    void* ptr = scalable_aligned_malloc(size, static_cast<size_t>(align));
    if (!ptr)
        throw std::bad_alloc{};

    return ptr;
}

void operator delete[](void* ptr, std::align_val_t /* align */) noexcept
{
    scalable_aligned_free(ptr);
}

//--------------------------------------------------------------------------------------------------

void* operator new[](size_t size, std::align_val_t align, std::nothrow_t const&) noexcept
{
    return scalable_aligned_malloc(size, static_cast<size_t>(align));
}

void operator delete[](void* ptr, std::align_val_t /* align */, std::nothrow_t const&) noexcept
{
    scalable_aligned_free(ptr);
}

#endif // defined(USE_TBB_ALLOCATOR)
