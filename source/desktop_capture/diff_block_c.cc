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

#include "desktop_capture/diff_block_c.h"

namespace aspia {

namespace {

const int kBytesPerPixel = 4;

} // namespace

uint8_t diffFullBlock_32x32_C(const uint8_t* image1, const uint8_t* image2, int bytes_per_row)
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

uint8_t diffFullBlock_16x16_C(const uint8_t* image1, const uint8_t* image2, int bytes_per_row)
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

uint8_t diffFullBlock_8x8_C(const uint8_t* image1, const uint8_t* image2, int bytes_per_row)
{
    static const int kBlockSize = 8;

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

} // namespace aspia
