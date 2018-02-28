//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop_frame_dib.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/desktop_frame_dib.h"
#include "base/logging.h"

namespace aspia {

DesktopFrameDIB::DesktopFrameDIB(const DesktopSize& size,
                                 const PixelFormat& format,
                                 int stride,
                                 uint8_t* data,
                                 HBITMAP bitmap)
    : DesktopFrame(size, format, stride, data),
      bitmap_(bitmap)
{
    // Nothing
}

// static
std::unique_ptr<DesktopFrameDIB>
DesktopFrameDIB::Create(const DesktopSize& size,
                        const PixelFormat& format,
                        HDC hdc)
{
    int bytes_per_row = size.Width() * format.BytesPerPixel();

    struct BitmapInfo
    {
        BITMAPINFOHEADER header;
        union
        {
            struct
            {
                uint32_t red;
                uint32_t green;
                uint32_t blue;
            } mask;
            RGBQUAD color[256];
        } u;
    };

    BitmapInfo bmi = { 0 };
    bmi.header.biSize      = sizeof(bmi.header);
    bmi.header.biBitCount  = format.BitsPerPixel();
    bmi.header.biSizeImage = bytes_per_row * size.Height();
    bmi.header.biPlanes    = 1;
    bmi.header.biWidth     = size.Width();
    bmi.header.biHeight    = -size.Height();

    if (format.BitsPerPixel() == 32 || format.BitsPerPixel() == 16)
    {
        bmi.header.biCompression = BI_BITFIELDS;

        bmi.u.mask.red   = format.RedMax()   << format.RedShift();
        bmi.u.mask.green = format.GreenMax() << format.GreenShift();
        bmi.u.mask.blue  = format.BlueMax()  << format.BlueShift();
    }
    else
    {
        bmi.header.biCompression = BI_RGB;

        for (uint32_t i = 0; i < 256; ++i)
        {
            const uint32_t red   = (i >> format.RedShift())   & format.RedMax();
            const uint32_t green = (i >> format.GreenShift()) & format.GreenMax();
            const uint32_t blue  = (i >> format.BlueShift())  & format.BlueMax();

            bmi.u.color[i].rgbRed   = static_cast<uint8_t>(red   * 0xFF / format.RedMax());
            bmi.u.color[i].rgbGreen = static_cast<uint8_t>(green * 0xFF / format.GreenMax());
            bmi.u.color[i].rgbBlue  = static_cast<uint8_t>(blue  * 0xFF / format.BlueMax());
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
        LOG(LS_ERROR) << "CreateDIBSection failed";
        return nullptr;
    }

    return std::unique_ptr<DesktopFrameDIB>(
        new DesktopFrameDIB(size,
                            format,
                            bytes_per_row,
                            reinterpret_cast<uint8_t*>(data),
                            bitmap));
}

HBITMAP DesktopFrameDIB::Bitmap()
{
    return bitmap_;
}

} // namespace aspia
