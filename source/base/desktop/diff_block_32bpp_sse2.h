//
// Aspia Project
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

#ifndef BASE_DESKTOP_DIFF_BLOCK_32BPP_SSE2_H
#define BASE_DESKTOP_DIFF_BLOCK_32BPP_SSE2_H

#include <QtGlobal>

namespace base {

#if defined(Q_PROCESSOR_X86)

quint8 diffFullBlock_32bpp_32x32_SSE2(
    const quint8* image1, const quint8* image2, int bytes_per_row);

quint8 diffFullBlock_32bpp_16x16_SSE2(
    const quint8* image1, const quint8* image2, int bytes_per_row);

#endif // defined(Q_PROCESSOR_X86)

} // namespace base

#endif // BASE_DESKTOP_DIFF_BLOCK_32BPP_SSE2_H
