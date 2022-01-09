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

#include "base/memory/scalable_allocator.h"

#include <vector>

namespace base {

template <class T>
using ScalableVector = std::vector<T, ScalableAllocator<T>>;

} // namespace base

#endif // BASE__MEMORY__SCALABLE_VECTOR_H