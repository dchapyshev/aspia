//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/desktop/diff_block_32bpp_c.h"

#include <cstring>

namespace base {

namespace {

const int kBytesPerPixel = 4;

} // namespace

//--------------------------------------------------------------------------------------------------
quint8 diffFullBlock_32bpp_32x32_C(
    const quint8* image1, const quint8* image2, int bytes_per_row)
{
    static const int kBlockSize = 32;

    for (int y = 0; y < kBlockSize; ++y)
    {
        if (memcmp(image1, image2, kBlockSize * kBytesPerPixel) != 0)
        {
            return 1U;
        }

        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0U;
}

//--------------------------------------------------------------------------------------------------
quint8 diffFullBlock_32bpp_16x16_C(
    const quint8* image1, const quint8* image2, int bytes_per_row)
{
    static const int kBlockSize = 16;

    for (int y = 0; y < kBlockSize; ++y)
    {
        if (memcmp(image1, image2, kBlockSize * kBytesPerPixel) != 0)
        {
            return 1U;
        }

        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0U;
}

} // namespace base
