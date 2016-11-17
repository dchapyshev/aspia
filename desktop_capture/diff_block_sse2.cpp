/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/diff_block_sse2.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "desktop_capture/diff_block_sse2.h"

#include <intrin.h>

uint8_t
DiffFullBlock_32x32_32bpp_SSE2(const uint8_t *image1, const uint8_t *image2, int bytes_per_row)
{
    __m128i acc = _mm_setzero_si128();
    __m128i v0;
    __m128i v1;
    __m128i sad;

    for (int y = 0; y < 32; ++y)
    {
        const __m128i* i1 = reinterpret_cast<const __m128i*>(image1);
        const __m128i* i2 = reinterpret_cast<const __m128i*>(image2);

        v0 = _mm_loadu_si128(i1);
        v1 = _mm_loadu_si128(i2);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        v0 = _mm_loadu_si128(i1 + 1);
        v1 = _mm_loadu_si128(i2 + 1);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        v0 = _mm_loadu_si128(i1 + 2);
        v1 = _mm_loadu_si128(i2 + 2);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        v0 = _mm_loadu_si128(i1 + 3);
        v1 = _mm_loadu_si128(i2 + 3);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        v0 = _mm_loadu_si128(i1 + 4);
        v1 = _mm_loadu_si128(i2 + 4);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        v0 = _mm_loadu_si128(i1 + 5);
        v1 = _mm_loadu_si128(i2 + 5);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        v0 = _mm_loadu_si128(i1 + 6);
        v1 = _mm_loadu_si128(i2 + 6);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        v0 = _mm_loadu_si128(i1 + 7);
        v1 = _mm_loadu_si128(i2 + 7);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        // This essential means sad = acc >> 64. We only care about the lower 16 bits.
        sad = _mm_shuffle_epi32(acc, 0xEE);
        sad = _mm_add_epi16(sad, acc);

        // Если строка имеет отличия
        int diff = _mm_cvtsi128_si32(sad);
        if (diff)
            return 1;

        // Переходим к следующей строке изображениях
        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0;
}

uint8_t
DiffFullBlock_32x32_16bpp_SSE2(const uint8_t *image1, const uint8_t *image2, int bytes_per_row)
{
    __m128i acc = _mm_setzero_si128();
    __m128i v0;
    __m128i v1;
    __m128i sad;

    for (int y = 0; y < 32; ++y)
    {
        const __m128i* i1 = reinterpret_cast<const __m128i*>(image1);
        const __m128i* i2 = reinterpret_cast<const __m128i*>(image2);

        v0 = _mm_loadu_si128(i1);
        v1 = _mm_loadu_si128(i2);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        v0 = _mm_loadu_si128(i1 + 1);
        v1 = _mm_loadu_si128(i2 + 1);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        v0 = _mm_loadu_si128(i1 + 2);
        v1 = _mm_loadu_si128(i2 + 2);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        v0 = _mm_loadu_si128(i1 + 3);
        v1 = _mm_loadu_si128(i2 + 3);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        // This essential means sad = acc >> 64. We only care about the lower 16 bits.
        sad = _mm_shuffle_epi32(acc, 0xEE);
        sad = _mm_adds_epu16(sad, acc);

        // Если строка имеет отличия
        int diff = _mm_cvtsi128_si32(sad);
        if (diff)
            return 1;

        // Переходим к следующей строке изображениях
        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0;
}

uint8_t
DiffFullBlock_16x16_32bpp_SSE2(const uint8_t *image1, const uint8_t *image2, int bytes_per_row)
{
    __m128i acc = _mm_setzero_si128();
    __m128i v0;
    __m128i v1;
    __m128i sad;

    for (int y = 0; y < 16; ++y)
    {
        const __m128i* i1 = reinterpret_cast<const __m128i*>(image1);
        const __m128i* i2 = reinterpret_cast<const __m128i*>(image2);

        v0 = _mm_loadu_si128(i1);
        v1 = _mm_loadu_si128(i2);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        v0 = _mm_loadu_si128(i1 + 1);
        v1 = _mm_loadu_si128(i2 + 1);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        v0 = _mm_loadu_si128(i1 + 2);
        v1 = _mm_loadu_si128(i2 + 2);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        v0 = _mm_loadu_si128(i1 + 3);
        v1 = _mm_loadu_si128(i2 + 3);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        // This essential means sad = acc >> 64. We only care about the lower 16 bits.
        sad = _mm_shuffle_epi32(acc, 0xEE);
        sad = _mm_add_epi16(sad, acc);

        // Если строка имеет отличия
        int diff = _mm_cvtsi128_si32(sad);
        if (diff)
            return 1;

        // Переходим к следующей строке изображениях
        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0;
}

uint8_t
DiffFullBlock_16x16_16bpp_SSE2(const uint8_t *image1, const uint8_t *image2, int bytes_per_row)
{
    __m128i acc = _mm_setzero_si128();
    __m128i v0;
    __m128i v1;
    __m128i sad;

    for (int y = 0; y < 16; ++y)
    {
        const __m128i* i1 = reinterpret_cast<const __m128i*>(image1);
        const __m128i* i2 = reinterpret_cast<const __m128i*>(image2);

        v0 = _mm_loadu_si128(i1);
        v1 = _mm_loadu_si128(i2);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        v0 = _mm_loadu_si128(i1 + 1);
        v1 = _mm_loadu_si128(i2 + 1);
        sad = _mm_sad_epu8(v0, v1);
        acc = _mm_add_epi16(acc, sad);

        // This essential means sad = acc >> 64. We only care about the lower 16 bits.
        sad = _mm_shuffle_epi32(acc, 0xEE);
        sad = _mm_adds_epu16(sad, acc);

        // Если строка имеет отличия
        int diff = _mm_cvtsi128_si32(sad);
        if (diff)
            return 1;

        // Переходим к следующей строке изображениях
        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0;
}
