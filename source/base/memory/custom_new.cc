//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#if defined(USE_MIMALLOC)

#include <new>
#include <mimalloc.h>

void* operator new(size_t size)
{
    return mi_new(size);
}

void operator delete(void* ptr) noexcept
{
    mi_free(ptr);
}

//--------------------------------------------------------------------------------------------------

void* operator new(size_t size, std::nothrow_t const&) noexcept
{
    return mi_new_nothrow(size);
}

void operator delete(void* ptr, std::nothrow_t const&) noexcept
{
    mi_free(ptr);
}

//--------------------------------------------------------------------------------------------------

void* operator new[](size_t size)
{
    return mi_new(size);
}

void operator delete[](void* ptr) noexcept
{
    mi_free(ptr);
}

//--------------------------------------------------------------------------------------------------

void* operator new[](size_t size, std::nothrow_t const&) noexcept
{
    return mi_new_nothrow(size);
}

void operator delete[](void* ptr, std::nothrow_t const&) noexcept
{
    mi_free(ptr);
}

//--------------------------------------------------------------------------------------------------

void* operator new(size_t size, std::align_val_t align)
{
    return mi_new_aligned(size, static_cast<size_t>(align));
}

void operator delete(void* ptr, std::align_val_t align) noexcept
{
    mi_free_aligned(ptr, static_cast<size_t>(align));
}

//--------------------------------------------------------------------------------------------------

void* operator new(size_t size, std::align_val_t align, std::nothrow_t const&) noexcept
{
    return mi_new_aligned_nothrow(size, static_cast<size_t>(align));
}

void operator delete(void* ptr, std::align_val_t align, std::nothrow_t const&) noexcept
{
    mi_free_aligned(ptr, static_cast<size_t>(align));
}

//--------------------------------------------------------------------------------------------------

void* operator new[](size_t size, std::align_val_t align)
{
    return mi_new_aligned(size, static_cast<size_t>(align));
}

void operator delete[](void* ptr, std::align_val_t align) noexcept
{
    mi_free_aligned(ptr, static_cast<size_t>(align));
}

//--------------------------------------------------------------------------------------------------

void* operator new[](size_t size, std::align_val_t align, std::nothrow_t const&) noexcept
{
    return mi_new_aligned_nothrow(size, static_cast<size_t>(align));
}

void operator delete[](void* ptr, std::align_val_t align, std::nothrow_t const&) noexcept
{
    mi_free_aligned(ptr, static_cast<size_t>(align));
}

#endif // defined(USE_MIMALLOC)
