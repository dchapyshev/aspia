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

#include "base/desktop/win/cursor.h"

#include "base/logging.h"
#include "base/desktop/mouse_cursor.h"
#include "base/win/scoped_gdi_object.h"

namespace base {

namespace {

constexpr uint32_t RGBA(uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    return (((a << 24) & 0xFF000000) | ((b << 16) & 0xFF0000) | ((g << 8) & 0xFF00) | (r & 0xFF));
}

constexpr int kBytesPerPixel = 4;

// Pixel colors used when generating cursor outlines.
constexpr uint32_t kPixelRgbaBlack       = RGBA(0,    0,    0,    0xFF);
constexpr uint32_t kPixelRgbaWhite       = RGBA(0xFF, 0xFF, 0xFF, 0xFF);
constexpr uint32_t kPixelRgbaTransparent = RGBA(0,    0,    0,    0);

constexpr uint32_t kPixelRgbWhite = RGB(0xFF, 0xFF, 0xFF);

//--------------------------------------------------------------------------------------------------
// Scans a 32bpp bitmap looking for any pixels with non-zero alpha component.
// Returns true if non-zero alpha is found. |stride| is expressed in pixels.
bool hasAlphaChannel(const uint32_t* data, int width, int height)
{
    const RGBQUAD* plane = reinterpret_cast<const RGBQUAD*>(data);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (plane->rgbReserved != 0)
                return true;

            ++plane;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
// Expands the cursor shape to add a white outline for visibility against dark backgrounds.
void addCursorOutline(int width, int height, uint32_t* data)
{
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            // If this is a transparent pixel (bgr == 0 and alpha = 0), check
            // the neighbor pixels to see if this should be changed to an
            // outline pixel.
            if (*data == kPixelRgbaTransparent)
            {
                // Change to white pixel if any neighbors (top, bottom, left,
                // right) are black.
                if ((y > 0 && data[-width] == kPixelRgbaBlack) ||
                    (y < height - 1 && data[width] == kPixelRgbaBlack) ||
                    (x > 0 && data[-1] == kPixelRgbaBlack) ||
                    (x < width - 1 && data[1] == kPixelRgbaBlack))
                {
                    *data = kPixelRgbaWhite;
                }
            }

            ++data;
        }
    }
}

//--------------------------------------------------------------------------------------------------
// Premultiplies RGB components of the pixel data in the given image by
// the corresponding alpha components.
void alphaMul(uint32_t* data, int width, int height)
{
    static_assert(sizeof(uint32_t) == kBytesPerPixel,
                  "size of uint32 should be the number of bytes per pixel");

    for (uint32_t* data_end = data + width * height; data != data_end; ++data)
    {
        RGBQUAD* from = reinterpret_cast<RGBQUAD*>(data);
        RGBQUAD* to = reinterpret_cast<RGBQUAD*>(data);

        to->rgbBlue  = static_cast<BYTE>((static_cast<quint16>(from->rgbBlue)  * from->rgbReserved) / 0xFF);
        to->rgbGreen = static_cast<BYTE>((static_cast<quint16>(from->rgbGreen) * from->rgbReserved) / 0xFF);
        to->rgbRed   = static_cast<BYTE>((static_cast<quint16>(from->rgbRed)   * from->rgbReserved) / 0xFF);
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// Converts an HCURSOR into a |MouseCursor| instance.
MouseCursor* mouseCursorFromHCursor(HDC dc, HCURSOR cursor)
{
    ICONINFO icon_info;
    memset(&icon_info, 0, sizeof(icon_info));

    if (!GetIconInfo(cursor, &icon_info))
    {
        PLOG(LS_ERROR) << "GetIconInfo failed";
        return nullptr;
    }

    // Make sure the bitmaps will be freed.
    ScopedHBITMAP scoped_mask(icon_info.hbmMask);
    ScopedHBITMAP scoped_color(icon_info.hbmColor);

    bool is_color = (icon_info.hbmColor != nullptr);

    // Get |scoped_mask| dimensions.
    BITMAP bitmap_info;

    if (!GetObjectW(scoped_mask, sizeof(bitmap_info), &bitmap_info))
    {
        PLOG(LS_ERROR) << "GetObjectW failed";
        return nullptr;
    }

    int width = bitmap_info.bmWidth;
    int height = bitmap_info.bmHeight;

    std::vector<uint32_t> mask_data(static_cast<size_t>(width) * static_cast<size_t>(height));

    // Get pixel data from |scoped_mask| converting it to 32bpp along the way.
    // GetDIBits() sets the alpha component of every pixel to 0.
    BITMAPV5HEADER bmi;
    memset(&bmi, 0, sizeof(bmi));

    bmi.bV5Size        = sizeof(bmi);
    bmi.bV5Width       = width;
    bmi.bV5Height      = -height; // request a top-down bitmap.
    bmi.bV5Planes      = 1;
    bmi.bV5BitCount    = kBytesPerPixel * 8;
    bmi.bV5Compression = BI_RGB;
    bmi.bV5AlphaMask   = 0xFF000000;
    bmi.bV5CSType      = LCS_WINDOWS_COLOR_SPACE;
    bmi.bV5Intent      = LCS_GM_BUSINESS;

    if (!GetDIBits(dc,
                   scoped_mask,
                   0,
                   static_cast<UINT>(height),
                   mask_data.data(),
                   reinterpret_cast<BITMAPINFO*>(&bmi),
                   DIB_RGB_COLORS))
    {
        PLOG(LS_ERROR) << "GetDIBits failed";
        return nullptr;
    }

    uint32_t* mask_plane = mask_data.data();
    QByteArray image;
    bool has_alpha = false;

    if (is_color)
    {
        image.resize(static_cast<size_t>(width) * static_cast<size_t>(height) *
                     static_cast<size_t>(kBytesPerPixel));

        // Get the pixels from the color bitmap.
        if (!GetDIBits(dc,
                       scoped_color,
                       0,
                       static_cast<UINT>(height),
                       image.data(),
                       reinterpret_cast<BITMAPINFO*>(&bmi),
                       DIB_RGB_COLORS))
        {
            PLOG(LS_ERROR) << "GetDIBits failed";
            return nullptr;
        }

        // GetDIBits() does not provide any indication whether the bitmap has
        // alpha channel, so we use hasAlphaChannel() below to find it out.
        has_alpha = hasAlphaChannel(reinterpret_cast<const uint32_t*>(image.data()), width, height);
    }
    else
    {
        // For non-color cursors, the mask contains both an AND and an XOR mask
        // and the height includes both. Thus, the width is correct, but we
        // need to divide by 2 to get the correct mask height.
        height /= 2;

        image.resize(static_cast<size_t>(width) * static_cast<size_t>(height) *
                     static_cast<size_t>(kBytesPerPixel));

        // The XOR mask becomes the color bitmap.
        memcpy(image.data(), mask_plane + (width * height), image.size());
    }

    // Reconstruct transparency from the mask if the color image does not has alpha channel.
    if (!has_alpha)
    {
        bool add_outline = false;
        uint32_t* dst = reinterpret_cast<uint32_t*>(image.data());
        uint32_t* mask = mask_plane;

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                // The two bitmaps combine as follows:
                //  mask  color   Windows Result   Our result    RGB   Alpha
                //   0     00      Black            Black         00    ff
                //   0     ff      White            White         ff    ff
                //   1     00      Screen           Transparent   00    00
                //   1     ff      Reverse-screen   Black         00    ff
                //
                // Since we don't support XOR cursors, we replace the "Reverse
                // Screen" with black. In this case, we also add an outline
                // around the cursor so that it is visible against a dark
                // background.
                if (*mask == kPixelRgbWhite)
                {
                    if (*dst != 0)
                    {
                        add_outline = true;
                        *dst = kPixelRgbaBlack;
                    }
                    else
                    {
                        *dst = kPixelRgbaTransparent;
                    }
                }
                else
                {
                    *dst = kPixelRgbaBlack ^ *dst;
                }

                ++dst;
                ++mask;
            }
        }

        if (add_outline)
        {
            addCursorOutline(width, height, reinterpret_cast<uint32_t*>(image.data()));
        }
    }

    // Pre-multiply the resulting pixels since MouseCursor uses premultiplied images.
    alphaMul(reinterpret_cast<uint32_t*>(image.data()), width, height);

    return new MouseCursor(std::move(image),
                           Size(width, height),
                           Point(static_cast<int>(icon_info.xHotspot),
                                 static_cast<int>(icon_info.yHotspot)));
}

} // namespace base
