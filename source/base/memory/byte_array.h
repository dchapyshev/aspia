//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__MEMORY__BYTE_ARRAY_H
#define BASE__MEMORY__BYTE_ARRAY_H

#include "build/build_config.h"

#if defined(USE_TBB)
#include <tbb/scalable_allocator.h>
#endif // defined(USE_TBB)

#include <cstdint>
#include <string>
#include <vector>

namespace base {

#if defined(USE_TBB)
using ByteArrayAllocator = tbb::scalable_allocator<uint8_t>;
#else // defined(USE_TBB)
using ByteArrayAllocator = std::allocator<uint8_t>;
#endif // defined(USE_*)

using ByteArray = std::vector<uint8_t, ByteArrayAllocator>;

ByteArray fromData(const void* data, size_t size);

ByteArray fromStdString(std::string_view in);
std::string toStdString(const ByteArray& in);

ByteArray fromHex(std::string_view in);
std::string toHex(const ByteArray& in);

std::ostream& operator<<(std::ostream& out, const ByteArray& bytearray);

} // namespace base

#endif // BASE__MEMORY__BYTE_ARRAY_H
