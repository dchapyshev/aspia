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

#ifndef BASE__DESKTOP__DIFF_BLOCK_32BPP_SSE2_H
#define BASE__DESKTOP__DIFF_BLOCK_32BPP_SSE2_H

#include "build/build_config.h"

#include <cstdint>

namespace base {

#if defined(ARCH_CPU_X86_FAMILY)

uint8_t diffFullBlock_32bpp_32x32_SSE2(
    const uint8_t* image1, const uint8_t* image2, int bytes_per_row);

uint8_t diffFullBlock_32bpp_16x16_SSE2(
    const uint8_t* image1, const uint8_t* image2, int bytes_per_row);

#endif // defined(ARCH_CPU_X86_FAMILY)

} // namespace base

#endif // BASE__DESKTOP__DIFF_BLOCK_32BPP_SSE2_H
