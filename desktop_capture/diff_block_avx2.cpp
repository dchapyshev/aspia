/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/diff_block_avx2.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "desktop_capture/diff_block_avx2.h"

#include <intrin.h>

namespace aspia {

uint8_t DiffFullBlock_32x32_AVX2(const uint8_t *image1, const uint8_t *image2, int bytes_per_row)
{
    __m256i acc = _mm256_setzero_si256();
    __m256i sad;
    __m128i sad128;

    for (int y = 0; y < 32; ++y)
    {
        const __m256i* i1 = reinterpret_cast<const __m256i*>(image1);
        const __m256i* i2 = reinterpret_cast<const __m256i*>(image2);

        sad = _mm256_sad_epu8(_mm256_load_si256(i1 + 0), _mm256_load_si256(i2 + 0));
        acc = _mm256_add_epi16(acc, sad);

        sad = _mm256_sad_epu8(_mm256_load_si256(i1 + 1), _mm256_load_si256(i2 + 1));
        acc = _mm256_add_epi16(acc, sad);

        sad = _mm256_sad_epu8(_mm256_load_si256(i1 + 2), _mm256_load_si256(i2 + 2));
        acc = _mm256_add_epi16(acc, sad);

        sad = _mm256_sad_epu8(_mm256_load_si256(i1 + 3), _mm256_load_si256(i2 + 3));
        acc = _mm256_add_epi16(acc, sad);

        // This essential means sad = acc >> 64. We only care about the lower 16 bits.
        sad = _mm256_shuffle_epi32(acc, 0xEE);
        sad = _mm256_add_epi16(sad, acc);

        sad128 = _mm256_extracti128_si256(sad, 1);
        sad128 = _mm_add_epi16(_mm256_castsi256_si128(sad), sad128);

        // Если строка имеет отличия
        int diff = _mm_cvtsi128_si32(sad128);
        if (diff)
            return 1;

        // Переходим к следующей строке изображения
        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0;
}

uint8_t DiffFullBlock_16x16_AVX2(const uint8_t *image1, const uint8_t *image2, int bytes_per_row)
{
    __m256i acc = _mm256_setzero_si256();
    __m256i sad;
    __m128i sad128;

    for (int y = 0; y < 16; ++y)
    {
        const __m256i* i1 = reinterpret_cast<const __m256i*>(image1);
        const __m256i* i2 = reinterpret_cast<const __m256i*>(image2);

        sad = _mm256_sad_epu8(_mm256_load_si256(i1 + 0), _mm256_load_si256(i2 + 0));
        acc = _mm256_add_epi16(acc, sad);

        sad = _mm256_sad_epu8(_mm256_load_si256(i1 + 1), _mm256_load_si256(i2 + 1));
        acc = _mm256_add_epi16(acc, sad);

        // This essential means sad = acc >> 64. We only care about the lower 16 bits.
        sad = _mm256_shuffle_epi32(acc, 0xEE);
        sad = _mm256_add_epi16(sad, acc);

        sad128 = _mm256_extracti128_si256(sad, 1);
        sad128 = _mm_add_epi16(_mm256_castsi256_si128(sad), sad128);

        // Если строка имеет отличия
        int diff = _mm_cvtsi128_si32(sad128);
        if (diff)
            return 1;

        // Переходим к следующей строке изображения
        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0;
}

} // namespace aspia
