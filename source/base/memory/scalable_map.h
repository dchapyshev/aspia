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

#ifndef BASE__MEMORY__SCALABLE_MAP_H
#define BASE__MEMORY__SCALABLE_MAP_H

#include "base/memory/scalable_allocator.h"

#include <map>

namespace base {

template <class Key, class T, class Compare = std::less<Key>>
using ScalableMap = std::map<Key, T, Compare, ScalableAllocator<std::pair<const Key, T>>>;

} // namespace base

#endif // BASE__MEMORY__SCALABLE_MAP_H
