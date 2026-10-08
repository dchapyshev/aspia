//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_LINUX_DRM_PIXEL_FORMAT_H
#define HOST_LINUX_DRM_PIXEL_FORMAT_H

#include <QSize>
#include <QtTypes>

#include <cstdint>

class DrmPixelFormat
{
public:
    // Returns true if |format| can be converted.
    static bool isSupported(uint32_t format);

    // Converts |size| pixels of |src| in |format| into |dst|. A negative |src_stride| reads the rows of
    // |src| bottom-up, |src| then pointing at the last row. Returns false for an unsupported format.
    static bool toBgra(const quint8* src, int src_stride, uint32_t format, const QSize& size, quint8* dst,
                       int dst_stride);
};

#endif // HOST_LINUX_DRM_PIXEL_FORMAT_H
