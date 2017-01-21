//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/dib_buffer.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/dib_buffer.h"

#include "base/exception.h"
#include "base/scoped_select_object.h"

namespace aspia {

DibBuffer::DibBuffer() :
    buffer_(nullptr),
    bytes_per_pixel_(0),
    bytes_per_row_(0)
{
    // Nothing
}

DibBuffer::~DibBuffer()
{
    // Nothing
}

void DibBuffer::Resize(HDC hdc, const DesktopSize &size, const PixelFormat &format)
{
    size_ = size;
    format_ = format;

    memory_dc_.set(CreateCompatibleDC(hdc));
    AllocateBuffer();

    bytes_per_pixel_ = format_.BytesPerPixel();
    bytes_per_row_ = bytes_per_pixel_ * size_.Width();
}

void DibBuffer::AllocateBuffer(int align)
{
    int aligned_width = ((size_.Width() + (align - 1)) / align) * 2;

    typedef struct
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
    } BitmapInfo;

    BitmapInfo bmi = { 0 };
    bmi.header.biSize      = sizeof(BITMAPINFOHEADER);
    bmi.header.biBitCount  = format_.BitsPerPixel();
    bmi.header.biSizeImage = format_.BytesPerPixel() * aligned_width * size_.Height();
    bmi.header.biPlanes    = 1;
    bmi.header.biWidth     = size_.Width();
    bmi.header.biHeight    = -size_.Height();

    if (format_.BitsPerPixel() == 8)
    {
        bmi.header.biCompression = BI_RGB;

        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t red   = (i >> format_.RedShift())   & format_.RedMax();
            uint32_t green = (i >> format_.GreenShift()) & format_.GreenMax();
            uint32_t blue  = (i >> format_.BlueShift())  & format_.BlueMax();

            bmi.u.color[i].rgbRed   = red   * 0xFF / format_.RedMax();
            bmi.u.color[i].rgbGreen = green * 0xFF / format_.GreenMax();
            bmi.u.color[i].rgbBlue  = blue  * 0xFF / format_.BlueMax();
        }
    }
    else
    {
        bmi.header.biCompression = ((format_.BitsPerPixel() == 24) ? BI_RGB : BI_BITFIELDS);

        bmi.u.mask.red   = format_.RedMax()   << format_.RedShift();
        bmi.u.mask.green = format_.GreenMax() << format_.GreenShift();
        bmi.u.mask.blue  = format_.BlueMax()  << format_.BlueShift();
    }

    bitmap_ = CreateDIBSection(memory_dc_,
                               reinterpret_cast<PBITMAPINFO>(&bmi),
                               DIB_RGB_COLORS,
                               reinterpret_cast<void**>(&buffer_),
                               nullptr,
                               0);
    if (!bitmap_)
    {
        LOG(ERROR) << "CreateDIBSection() failed: " << GetLastError();
        throw Exception("Unable to create screen buffer");
    }
}

const DesktopSize& DibBuffer::Size() const
{
    return size_;
}

const PixelFormat& DibBuffer::Format() const
{
    return format_;
}

void DibBuffer::DrawTo(HDC target_dc,
                       const DesktopPoint &dest,
                       const DesktopPoint &src,
                       const DesktopSize &size)
{
    ScopedSelectObject select_object(memory_dc_, bitmap_);

    BitBlt(target_dc,
           dest.x(), dest.y(),
           size.Width(), size.Height(),
           memory_dc_,
           src.x(), src.y(),
           SRCCOPY);
}

void DibBuffer::DrawFrom(HDC source_dc,
                         const DesktopPoint &dest,
                         const DesktopPoint &src,
                         const DesktopSize &size)
{
    ScopedSelectObject select_object(memory_dc_, bitmap_);

    BitBlt(memory_dc_,
           dest.x(), dest.y(),
           size.Width(), size.Height(),
           source_dc,
           src.x(), src.y(),
           CAPTUREBLT | SRCCOPY);
}

void DibBuffer::Copy(const uint8_t *src_buffer, uint8_t *dst_buffer, const DesktopRect &rect)
{
    const uint8_t *src = src_buffer + bytes_per_row_ * rect.y() + rect.x() * bytes_per_pixel_;
    uint8_t *dst = dst_buffer + bytes_per_row_ * rect.y() + rect.x() * bytes_per_pixel_;

    int height = rect.Height();
    int row_size = rect.Width() * bytes_per_pixel_;

    for (int y = 0; y < height; ++y)
    {
        memcpy(dst, src, row_size);

        src += bytes_per_row_;
        dst += bytes_per_row_;
    }
}

void DibBuffer::CopyFrom(const uint8_t *buffer, const DesktopRect &rect)
{
    Copy(buffer, buffer_, rect);
}

void DibBuffer::CopyFrom(const uint8_t *buffer, const DesktopRegion &region)
{
    for (DesktopRegion::Iterator iter(region); !iter.IsAtEnd(); iter.Advance())
    {
        CopyFrom(buffer, iter.rect());
    }
}

void DibBuffer::CopyTo(uint8_t *buffer, const DesktopRect &rect)
{
    Copy(buffer_, buffer, rect);
}

void DibBuffer::CopyTo(uint8_t *buffer, const DesktopRegion &region)
{
    for (DesktopRegion::Iterator iter(region); !iter.IsAtEnd(); iter.Advance())
    {
        CopyTo(buffer, iter.rect());
    }
}

bool DibBuffer::Contains(int32_t x, int32_t y)
{
    return (x > 0 && x <= size_.Width() && y > 0 && y <= size_.Height());
}

} // namespace aspia
