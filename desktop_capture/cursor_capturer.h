/*
* PROJECT:         Aspia Remote Desktop
* FILE:            server/cursor_capturer.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CURSOR_CAPTURER_H
#define _ASPIA_CURSOR_CAPTURER_H

#include "base/logging.h"

class CursorCapturer
{
public:
    CursorCapturer() {}
    ~CursorCapturer() {}

    bool IsInitialized() const { return true; }

    bool Initialize();

    const uint8_t* Process()
    {
        CURSORINFO CursorInfo = { 0 };

        CursorInfo.cbSize = sizeof(CURSORINFO);
        if (!GetCursorInfo(&CursorInfo))
        {
            LOG(ERROR) << "GetCursorInfo() failed: " << GetLastError();
        }
        else
        {
            HCURSOR hNewCursor = CursorInfo.hCursor;

            if (hNewCursor != hOldCursor_)
            {
                hOldCursor_ = hNewCursor;
            }
        }
    }

private:
    HBITMAP GetCursorColorBits(uint8_t **ColorBits, HCURSOR hCursor, int32_t Width, int32_t Height, int32_t BitsPerPixel)
    {
        Bitmap::BitmapInfo BitmapInfo = { 0 };
        HBITMAP hMemoryBitmap;

        Bitmap::FillInfo(&BitmapInfo, Width, Height, BitsPerPixel);

        hMemoryBitmap = CreateDIBSection(hMemoryDC_,
                                         reinterpret_cast<PBITMAPINFO>(&BitmapInfo),
                                         DIB_RGB_COLORS,
                                         reinterpret_cast<LPVOID*>(ColorBits),
                                         nullptr, 0);
        if (hMemoryBitmap)
        {
            HBITMAP hOldBitmap;

            hOldBitmap = reinterpret_cast<HBITMAP>(SelectObject(hMemoryDC_, hMemoryBitmap));
            if (hOldBitmap)
            {
                DrawIconEx(hMemoryDC_, 0, 0, hCursor, 0, 0, 0, nullptr, DI_IMAGE);
                SelectObject(hMemoryDC_, hOldBitmap);
            }
        }

        return hMemoryBitmap;
    }

    HBITMAP GetCursorBits(HCURSOR hCursor,
                          uint8_t **MaskBits, size_t *MaskSize,
                          uint8_t **ColorBits, size_t *ColorSize,
                          uint32_t *xHotSpot, uint32_t *yHotSpot,
                          int32_t *Width, int32_t *Height,
                          int32_t BitsPerPixel)
    {
        ICONINFO IconInfo = { 0 };

        if (GetIconInfo(hCursor, &IconInfo))
        {
            BITMAP BitmapMask;

            GetObjectW(IconInfo.hbmMask, sizeof(BITMAP), reinterpret_cast<LPVOID>(&BitmapMask));

            *MaskSize = BitmapMask.bmWidthBytes * BitmapMask.bmHeight;
            *MaskBits = reinterpret_cast<uint8_t*>(malloc(*MaskSize));

            if (*MaskBits)
            {
                if (GetBitmapBits(IconInfo.hbmMask, *MaskSize, *MaskBits))
                {
                    HBITMAP hMemoryBitmap;

                    *Width = BitmapMask.bmWidth;
                    *Height = IconInfo.hbmColor ? BitmapMask.bmHeight : BitmapMask.bmHeight / 2;

                    *ColorSize = (*Width) * (*Height) * (BitsPerPixel / 8);

                    hMemoryBitmap = GetCursorColorBits(ColorBits, hCursor, *Width, *Height, BitsPerPixel);
                    if (hMemoryBitmap)
                    {
                        if (IconInfo.hbmColor)
                            DeleteObject(IconInfo.hbmColor);
                        if (IconInfo.hbmMask)
                            DeleteObject(IconInfo.hbmMask);

                        *xHotSpot = IconInfo.xHotspot;
                        *yHotSpot = IconInfo.yHotspot;

                        return hMemoryBitmap;
                    }
                }

                free(*MaskBits);
            }

            if (IconInfo.hbmColor)
                DeleteObject(IconInfo.hbmColor);
            if (IconInfo.hbmMask)
                DeleteObject(IconInfo.hbmMask);
        }

        return nullptr;
    }

private:
    HDC hDesktopDC_;
    HDC hMemoryDC_;
    HCURSOR hOldCursor_;
};

#endif // _ASPIA_CURSOR_CAPTURER_H
