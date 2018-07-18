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

#include "desktop_capture/desktop_frame_dib.h"

namespace aspia {

DesktopFrameDIB::DesktopFrameDIB(const QSize& size,
                                 const PixelFormat& format,
                                 int stride,
                                 quint8* data,
                                 HBITMAP bitmap)
    : DesktopFrame(size, format, stride, data),
      bitmap_(bitmap)
{
    // Nothing
}

// static
std::unique_ptr<DesktopFrameDIB>
DesktopFrameDIB::create(const QSize& size,
                        const PixelFormat& format,
                        HDC hdc)
{
    int bytes_per_row = size.width() * format.bytesPerPixel();

    struct BitmapInfo
    {
        BITMAPINFOHEADER header;
        union
        {
            struct
            {
                quint32 red;
                quint32 green;
                quint32 blue;
            } mask;
            RGBQUAD color[256];
        } u;
    };

    BitmapInfo bmi = { 0 };
    bmi.header.biSize      = sizeof(bmi.header);
    bmi.header.biBitCount  = format.bitsPerPixel();
    bmi.header.biSizeImage = bytes_per_row * size.height();
    bmi.header.biPlanes    = 1;
    bmi.header.biWidth     = size.width();
    bmi.header.biHeight    = -size.height();

    if (format.bitsPerPixel() == 32 || format.bitsPerPixel() == 16)
    {
        bmi.header.biCompression = BI_BITFIELDS;

        bmi.u.mask.red   = format.redMax()   << format.redShift();
        bmi.u.mask.green = format.greenMax() << format.greenShift();
        bmi.u.mask.blue  = format.blueMax()  << format.blueShift();
    }
    else
    {
        bmi.header.biCompression = BI_RGB;

        for (quint32 i = 0; i < 256; ++i)
        {
            const quint32 red   = (i >> format.redShift())   & format.redMax();
            const quint32 green = (i >> format.greenShift()) & format.greenMax();
            const quint32 blue  = (i >> format.blueShift())  & format.blueMax();

            bmi.u.color[i].rgbRed   = static_cast<quint8>(red   * 0xFF / format.redMax());
            bmi.u.color[i].rgbGreen = static_cast<quint8>(green * 0xFF / format.greenMax());
            bmi.u.color[i].rgbBlue  = static_cast<quint8>(blue  * 0xFF / format.blueMax());
        }
    }

    void* data = nullptr;

    HBITMAP bitmap = CreateDIBSection(hdc,
                                      reinterpret_cast<LPBITMAPINFO>(&bmi),
                                      DIB_RGB_COLORS,
                                      &data,
                                      nullptr,
                                      0);
    if (!bitmap)
    {
        qWarning("CreateDIBSection failed");
        return nullptr;
    }

    return std::unique_ptr<DesktopFrameDIB>(
        new DesktopFrameDIB(size,
                            format,
                            bytes_per_row,
                            reinterpret_cast<quint8*>(data),
                            bitmap));
}

} // namespace aspia
