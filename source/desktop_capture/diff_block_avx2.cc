//
// PROJECT:         Aspia
// FILE:            desktop_capture/diff_block_avx2.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/diff_block_avx2.h"

#if defined(Q_CC_MSVC)
#include <intrin.h>
#else
#include <mmintrin.h>
#include <emmintrin.h>
#endif

namespace aspia {

quint8 diffFullBlock_32x2_AVX2(const quint8* image1, const quint8* image2, int bytes_per_row)
{
    __m256i acc = _mm256_setzero_si256();
    __m256i sad;
    __m128i sad128;
    const __m256i* i1;
    const __m256i* i2;

    i1 = reinterpret_cast<const __m256i*>(image1);
    i2 = reinterpret_cast<const __m256i*>(image2);

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

    i1 = reinterpret_cast<const __m256i*>(image1 + bytes_per_row);
    i2 = reinterpret_cast<const __m256i*>(image2 + bytes_per_row);

    sad = _mm256_sad_epu8(_mm256_loadu_si256(i1 + 0), _mm256_loadu_si256(i2 + 0));
    acc = _mm256_adds_epu16(acc, sad);

    sad = _mm256_sad_epu8(_mm256_loadu_si256(i1 + 1), _mm256_loadu_si256(i2 + 1));
    acc = _mm256_adds_epu16(acc, sad);

    sad = _mm256_sad_epu8(_mm256_loadu_si256(i1 + 2), _mm256_loadu_si256(i2 + 2));
    acc = _mm256_adds_epu16(acc, sad);

    sad = _mm256_sad_epu8(_mm256_loadu_si256(i1 + 3), _mm256_loadu_si256(i2 + 3));
    acc = _mm256_adds_epu16(acc, sad);

    sad = _mm256_srli_si256(acc, 8);
    sad = _mm256_add_epi32(acc, sad);
    sad128 = _mm256_extracti128_si256(sad, 1);
    sad128 = _mm_add_epi32(_mm256_castsi256_si128(sad), sad128);

    if (_mm_cvtsi128_si32(sad128))
        return 1U;

    return 0U;
}

} // namespace aspia
