//
// PROJECT:         Aspia
// FILE:            desktop_capture/diff_block_sse3.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DIFF_BLOCK_SSE3_H
#define _ASPIA_DESKTOP_CAPTURE__DIFF_BLOCK_SSE3_H

namespace aspia {

quint8 diffFullBlock_32x32_SSE3(const quint8* image1, const quint8* image2, int bytes_per_row);

quint8 diffFullBlock_16x16_SSE3(const quint8* image1, const quint8* image2, int bytes_per_row);

quint8 diffFullBlock_8x8_SSE3(const quint8* image1, const quint8* image2, int bytes_per_row);

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DIFF_BLOCK_SSE3_H
