//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_DESKTOP_CAPTURE__DIFF_BLOCK_AVX2_H
#define ASPIA_DESKTOP_CAPTURE__DIFF_BLOCK_AVX2_H

#include <cstdint>

namespace desktop {

uint8_t diffFullBlock_32x32_AVX2(const uint8_t* image1, const uint8_t* image2, int bytes_per_row);

uint8_t diffFullBlock_16x16_AVX2(const uint8_t* image1, const uint8_t* image2, int bytes_per_row);

uint8_t diffFullBlock_8x8_AVX2(const uint8_t* image1, const uint8_t* image2, int bytes_per_row);

} // namespace desktop

#endif // ASPIA_DESKTOP_CAPTURE__DIFF_BLOCK_AVX2_H
