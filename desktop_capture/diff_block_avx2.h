/*
* PROJECT:         Aspia Remote Desktop
* FILE:            server/diff_block_avx2.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_DESKTOP_CAPTURE__DIFF_BLOCK_AVX2_H
#define _ASPIA_DESKTOP_CAPTURE__DIFF_BLOCK_AVX2_H

#include "aspia_config.h"

#include <stdint.h>

namespace aspia {

uint8_t DiffFullBlock_32x32_AVX2(const uint8_t *image1, const uint8_t *image2, int bytes_per_row);

uint8_t DiffFullBlock_16x16_AVX2(const uint8_t *image1, const uint8_t *image2, int bytes_per_row);

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DIFF_BLOCK_AVX2_H
