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

#include "desktop_capture/diff_block_avx2.h"
#include "build_config.h"

#if defined(CC_MSVC)
#include <intrin.h>
#else
#include <mmintrin.h>
#include <emmintrin.h>
#endif

namespace aspia {

uint8_t diffFullBlock_32x32_AVX2(const uint8_t* image1, const uint8_t* image2, int bytes_per_row)
{
    __m256i acc = _mm256_setzero_si256();
    __m256i sad;
    __m128i sad128;

    for (int i = 0; i < 32; ++i)
    {
        const __m256i* i1 = reinterpret_cast<const __m256i*>(image1);
        const __m256i* i2 = reinterpret_cast<const __m256i*>(image2);

        sad = _mm256_sad_epu8(_mm256_loadu_si256(i1 + 0), _mm256_loadu_si256(i2 + 0));
        acc = _mm256_add_epi32(acc, sad);

        sad = _mm256_sad_epu8(_mm256_loadu_si256(i1 + 1), _mm256_loadu_si256(i2 + 1));
        acc = _mm256_add_epi32(acc, sad);

        sad = _mm256_sad_epu8(_mm256_loadu_si256(i1 + 2), _mm256_loadu_si256(i2 + 2));
        acc = _mm256_add_epi32(acc, sad);

        sad = _mm256_sad_epu8(_mm256_loadu_si256(i1 + 3), _mm256_loadu_si256(i2 + 3));
        acc = _mm256_add_epi32(acc, sad);

        sad = _mm256_srli_si256(acc, 8);
        sad = _mm256_add_epi32(acc, sad);
        sad128 = _mm256_extracti128_si256(sad, 1);
        sad128 = _mm_add_epi32(_mm256_castsi256_si128(sad), sad128);

        if (_mm_cvtsi128_si32(sad128))
            return 1U;

        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0U;
}

uint8_t diffFullBlock_16x16_AVX2(const uint8_t* image1, const uint8_t* image2, int bytes_per_row)
{
    __m256i acc = _mm256_setzero_si256();
    __m256i sad;
    __m128i sad128;

    for (int i = 0; i < 16; ++i)
    {
        const __m256i* i1 = reinterpret_cast<const __m256i*>(image1);
        const __m256i* i2 = reinterpret_cast<const __m256i*>(image2);

        sad = _mm256_sad_epu8(_mm256_loadu_si256(i1 + 0), _mm256_loadu_si256(i2 + 0));
        acc = _mm256_add_epi32(acc, sad);

        sad = _mm256_sad_epu8(_mm256_loadu_si256(i1 + 1), _mm256_loadu_si256(i2 + 1));
        acc = _mm256_add_epi32(acc, sad);

        sad = _mm256_srli_si256(acc, 8);
        sad = _mm256_add_epi32(acc, sad);
        sad128 = _mm256_extracti128_si256(sad, 1);
        sad128 = _mm_add_epi32(_mm256_castsi256_si128(sad), sad128);

        if (_mm_cvtsi128_si32(sad128))
            return 1U;

        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0U;
}

uint8_t diffFullBlock_8x8_AVX2(const uint8_t* image1, const uint8_t* image2, int bytes_per_row)
{
    __m256i acc = _mm256_setzero_si256();
    __m256i sad;
    __m128i sad128;

    for (int i = 0; i < 8; ++i)
    {
        const __m256i* i1 = reinterpret_cast<const __m256i*>(image1);
        const __m256i* i2 = reinterpret_cast<const __m256i*>(image2);

        sad = _mm256_sad_epu8(_mm256_loadu_si256(i1 + 0), _mm256_loadu_si256(i2 + 0));
        acc = _mm256_add_epi32(acc, sad);

        sad = _mm256_srli_si256(acc, 8);
        sad = _mm256_add_epi32(acc, sad);
        sad128 = _mm256_extracti128_si256(sad, 1);
        sad128 = _mm_add_epi32(_mm256_castsi256_si128(sad), sad128);

        if (_mm_cvtsi128_si32(sad128))
            return 1U;

        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0U;
}

} // namespace aspia
