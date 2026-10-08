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

#include "host/linux/drm_pixel_format.h"

#include <drm/drm_fourcc.h>
#include <libyuv/convert_argb.h>
#include <libyuv/planar_functions.h>

#include "base/logging.h"

namespace {

using ConvertFunc = int (*)(const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int width,
                            int height);

//--------------------------------------------------------------------------------------------------
ConvertFunc convertFunc(uint32_t format)
{
    switch (format)
    {
        case DRM_FORMAT_XRGB8888:
        case DRM_FORMAT_ARGB8888:  // memory B,G,R,A - already the target layout
            return libyuv::ARGBCopy;
        case DRM_FORMAT_XBGR8888:
        case DRM_FORMAT_ABGR8888:  // memory R,G,B,A
            return libyuv::ABGRToARGB;
        case DRM_FORMAT_RGBX8888:
        case DRM_FORMAT_RGBA8888:  // memory A,B,G,R
            return libyuv::RGBAToARGB;
        case DRM_FORMAT_BGRX8888:
        case DRM_FORMAT_BGRA8888:  // memory A,R,G,B
            return libyuv::BGRAToARGB;
        case DRM_FORMAT_XRGB2101010:
        case DRM_FORMAT_ARGB2101010:  // 2-bit alpha + 10-bit R,G,B
            return libyuv::AR30ToARGB;
        case DRM_FORMAT_XBGR2101010:
        case DRM_FORMAT_ABGR2101010:  // 2-bit alpha + 10-bit B,G,R
            return libyuv::AB30ToARGB;
        case DRM_FORMAT_RGB565:
            return libyuv::RGB565ToARGB;
        default:
            return nullptr;
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool DrmPixelFormat::isSupported(uint32_t format)
{
    return convertFunc(format) != nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
bool DrmPixelFormat::toBgra(const quint8* src, int src_stride, uint32_t format, const QSize& size,
                            quint8* dst, int dst_stride)
{
    ConvertFunc func = convertFunc(format);
    if (!func)
    {
        LOG(ERROR) << "Unsupported pixel format:" << Qt::hex << format;
        return false;
    }

    return func(src, src_stride, dst, dst_stride, size.width(), size.height()) == 0;
}
