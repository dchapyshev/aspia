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

#ifndef _ASPIA_DESKTOP_CAPTURE__DIFF_BLOCK_AVX2_H
#define _ASPIA_DESKTOP_CAPTURE__DIFF_BLOCK_AVX2_H

namespace aspia {

quint8 diffFullBlock_32x32_AVX2(const quint8* image1, const quint8* image2, int bytes_per_row);

quint8 diffFullBlock_16x16_AVX2(const quint8* image1, const quint8* image2, int bytes_per_row);

quint8 diffFullBlock_8x8_AVX2(const quint8* image1, const quint8* image2, int bytes_per_row);

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DIFF_BLOCK_AVX2_H
