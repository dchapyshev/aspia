//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/cursor_capturer.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/cursor_capturer.h"

#include "base/logging.h"

namespace aspia {

static const int kBytesPerPixel = 4;

CursorCapturer::CursorCapturer()
{
    desktop_dc_.reset(new ScopedGetDC(nullptr));

    memory_dc_.set(CreateCompatibleDC(*desktop_dc_));
}

CursorCapturer::~CursorCapturer()
{
    // Nothing
}

const uint8_t* CursorCapturer::Process()
{
    CURSORINFO cursor_info = { 0 };

    cursor_info.cbSize = sizeof(CURSORINFO);
    if (!GetCursorInfo(&cursor_info))
    {
        LOG(ERROR) << "GetCursorInfo() failed: " << GetLastError();
    }
    else
    {
        HCURSOR cursor = cursor_info.hCursor;

        if (cursor != prev_cursor_)
        {
            prev_cursor_ = cursor;
        }
    }

    return nullptr;
}

HBITMAP CursorCapturer::GetCursorColorBits(uint8_t **bits, HCURSOR cursor, const DesktopSize &size)
{
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

    bmi.header.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.header.biBitCount    = kBytesPerPixel * 8;
    bmi.header.biSizeImage   = kBytesPerPixel * size.Width() * size.Height();
    bmi.header.biPlanes      = 1;
    bmi.header.biWidth       = size.Width();
    bmi.header.biHeight      = -size.Height();
    bmi.header.biCompression = BI_BITFIELDS;
    bmi.u.mask.red           = 0x00FF0000;
    bmi.u.mask.green         = 0x0000FF00;
    bmi.u.mask.blue          = 0x000000FF;

    ScopedBitmap bitmap(CreateDIBSection(memory_dc_,
                                         reinterpret_cast<PBITMAPINFO>(&bmi),
                                         DIB_RGB_COLORS,
                                         reinterpret_cast<LPVOID*>(bits),
                                         nullptr,
                                         0));
    if (bitmap)
    {
        ScopedSelectObject(memory_dc_, bitmap);

        DrawIconEx(memory_dc_, 0, 0, cursor, 0, 0, 0, nullptr, DI_IMAGE);
    }

    return bitmap.release();
}

HBITMAP CursorCapturer::GetCursorBits(HCURSOR cursor,
                                      uint8_t **MaskBits, size_t *MaskSize,
                                      uint8_t **ColorBits, size_t *ColorSize,
                                      DesktopPoint &hotspot,
                                      DesktopSize &size)
{
    ICONINFO icon_info = { 0 };

    if (GetIconInfo(cursor, &icon_info))
    {
        ScopedBitmap color(icon_info.hbmColor);
        ScopedBitmap mask(icon_info.hbmMask);

        BITMAP mask_bitmap;

        GetObjectW(mask, sizeof(mask_bitmap), reinterpret_cast<LPVOID>(&mask_bitmap));

        *MaskSize = mask_bitmap.bmWidthBytes * mask_bitmap.bmHeight;
        *MaskBits = reinterpret_cast<uint8_t*>(malloc(*MaskSize));

        if (*MaskBits)
        {
            if (GetBitmapBits(mask, *MaskSize, *MaskBits))
            {
                size.Set(mask_bitmap.bmWidth,
                         color ? mask_bitmap.bmHeight : mask_bitmap.bmHeight / 2);

                *ColorSize = size.Width() * size.Height() * kBytesPerPixel;

                HBITMAP bitmap(GetCursorColorBits(ColorBits, cursor, size));
                if (bitmap)
                {
                    hotspot.Set(icon_info.xHotspot, icon_info.yHotspot);

                    return bitmap;
                }
            }

            free(*MaskBits);
        }
    }

    return nullptr;
}

} // namespace aspia
