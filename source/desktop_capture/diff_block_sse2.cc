//
// PROJECT:         Aspia
// FILE:            desktop_capture/diff_block_sse2.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/diff_block_sse2.h"

#include <intrin.h>

namespace aspia {

quint8 diffFullBlock_32x32_SSE2(const quint8* image1, const quint8* image2, int bytes_per_row)
{
    __m128i acc = _mm_setzero_si128();
    __m128i sad;

    for (int y = 0; y < 32; ++y)
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

        // Go to the next row of the image.
        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0U;
}

quint8 diffFullBlock_16x16_SSE2(const quint8* image1, const quint8* image2, int bytes_per_row)
{
    __m128i acc = _mm_setzero_si128();
    __m128i sad;

    for (int y = 0; y < 16; ++y)
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

        // Go to the next row of the image.
        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0U;
}

} // namespace aspia
