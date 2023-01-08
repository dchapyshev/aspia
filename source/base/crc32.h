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

#ifndef BASE_CRC32_H
#define BASE_CRC32_H

#include <cstdint>
#include <cstddef>

namespace base {

extern const uint32_t kCrcTable[256];

// This provides a simple, fast CRC-32 calculation that can be used for checking the integrity of
// data. It is not a "secure" calculation! |sum| can start with any seed or be used to continue an
// operation began with previous data.
uint32_t crc32(uint32_t sum, const void* data, size_t size);

} // namespace base

#endif // BASE_CRC32_H
