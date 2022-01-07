//
// Aspia Project
// Copyright (C) 2021 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__MEMORY__SCALABLE_VECTOR_H
#define BASE__MEMORY__SCALABLE_VECTOR_H

#include <vector>

#if defined(USE_TBB_ALLOCATOR)
#include <tbb/scalable_allocator.h>
#endif // defined(USE_TBB_ALLOCATOR)

namespace base {

template <class T>
#if defined(USE_TBB_ALLOCATOR)
using ScalableAllocator = tbb::scalable_allocator<T>;
#else // defined(USE_TBB_ALLOCATOR)
using ScalableAllocator = std::allocator<T>;
#endif // defined(USE_*)

template <class T>
using ScalableVector = std::vector<T, ScalableAllocator<T>>;

} // namespace base

#endif // BASE__MEMORY__SCALABLE_VECTOR_H
