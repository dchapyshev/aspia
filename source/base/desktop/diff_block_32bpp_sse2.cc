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

#include "base/desktop/diff_block_32bpp_sse2.h"

#if defined(Q_PROCESSOR_X86)
#if defined(Q_CC_MSVC)
#include <intrin.h>
#else
#include <mmintrin.h>
#include <emmintrin.h>
#endif // defined(Q_CC_*)
#endif // defined(Q_PROCESSOR_X86)

namespace base {

#if defined(Q_PROCESSOR_X86)

//--------------------------------------------------------------------------------------------------
quint8 diffFullBlock_32bpp_32x32_SSE2(
    const quint8* image1, const quint8* image2, int bytes_per_row)
{
    __m128i acc = _mm_setzero_si128();
    __m128i sad;

    for (int i = 0; i < 32; ++i)
    {
        const __m128i* i1 = reinterpret_cast<const __m128i*>(image1);
        const __m128i* i2 = reinterpret_cast<const __m128i*>(image2);

        sad = _mm_sad_epu8(_mm_loadu_si128(i1 + 0), _mm_loadu_si128(i2 + 0));
        acc = _mm_adds_epu16(acc, sad);

        sad = _mm_sad_epu8(_mm_loadu_si128(i1 + 1), _mm_loadu_si128(i2 + 1));
        acc = _mm_adds_epu16(acc, sad);

        sad = _mm_sad_epu8(_mm_loadu_si128(i1 + 2), _mm_loadu_si128(i2 + 2));
        acc = _mm_adds_epu16(acc, sad);

        sad = _mm_sad_epu8(_mm_loadu_si128(i1 + 3), _mm_loadu_si128(i2 + 3));
        acc = _mm_adds_epu16(acc, sad);

        sad = _mm_sad_epu8(_mm_loadu_si128(i1 + 4), _mm_loadu_si128(i2 + 4));
        acc = _mm_adds_epu16(acc, sad);

        sad = _mm_sad_epu8(_mm_loadu_si128(i1 + 5), _mm_loadu_si128(i2 + 5));
        acc = _mm_adds_epu16(acc, sad);

        sad = _mm_sad_epu8(_mm_loadu_si128(i1 + 6), _mm_loadu_si128(i2 + 6));
        acc = _mm_adds_epu16(acc, sad);

        sad = _mm_sad_epu8(_mm_loadu_si128(i1 + 7), _mm_loadu_si128(i2 + 7));
        acc = _mm_adds_epu16(acc, sad);

        // This essential means sad = acc >> 64. We only care about the lower 16 bits.
        sad = _mm_shuffle_epi32(acc, 0xEE);
        sad = _mm_adds_epu16(sad, acc);

        // If the row has differences.
        if (_mm_cvtsi128_si32(sad))
            return 1U;

        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0U;
}

//--------------------------------------------------------------------------------------------------
quint8 diffFullBlock_32bpp_16x16_SSE2(
    const quint8* image1, const quint8* image2, int bytes_per_row)
{
    __m128i acc = _mm_setzero_si128();
    __m128i sad;

    for (int i = 0; i < 16; ++i)
    {
        const __m128i* i1 = reinterpret_cast<const __m128i*>(image1);
        const __m128i* i2 = reinterpret_cast<const __m128i*>(image2);

        sad = _mm_sad_epu8(_mm_loadu_si128(i1 + 0), _mm_loadu_si128(i2 + 0));
        acc = _mm_adds_epu16(acc, sad);

        sad = _mm_sad_epu8(_mm_loadu_si128(i1 + 1), _mm_loadu_si128(i2 + 1));
        acc = _mm_adds_epu16(acc, sad);

        sad = _mm_sad_epu8(_mm_loadu_si128(i1 + 2), _mm_loadu_si128(i2 + 2));
        acc = _mm_adds_epu16(acc, sad);

        sad = _mm_sad_epu8(_mm_loadu_si128(i1 + 3), _mm_loadu_si128(i2 + 3));
        acc = _mm_adds_epu16(acc, sad);

        // This essential means sad = acc >> 64. We only care about the lower 16 bits.
        sad = _mm_shuffle_epi32(acc, 0xEE);
        sad = _mm_adds_epu16(sad, acc);

        // If the row has differences.
        if (_mm_cvtsi128_si32(sad))
            return 1U;

        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0U;
}

#endif // defined(Q_PROCESSOR_X86)

} // namespace base
