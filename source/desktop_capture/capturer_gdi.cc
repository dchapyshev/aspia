//
// PROJECT:         Aspia
// FILE:            desktop_capture/capturer_gdi.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/capturer_gdi.h"

#include <dwmapi.h>

#include "base/logging.h"
#include "base/scoped_select_object.h"

namespace aspia {

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

// Scans a 32bpp bitmap looking for any pixels with non-zero alpha component.
// Returns true if non-zero alpha is found. |stride| is expressed in pixels.
bool HasAlphaChannel(const uint32_t* data, int width, int height)
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

// Expands the cursor shape to add a white outline for visibility against
// dark backgrounds.
void AddCursorOutline(int width, int height, uint32_t* data)
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

// Premultiplies RGB components of the pixel data in the given image by
// the corresponding alpha components.
void AlphaMul(uint32_t* data, int width, int height)
{
    static_assert(sizeof(uint32_t) == kBytesPerPixel,
                  "size of uint32 should be the number of bytes per pixel");

    for (uint32_t* data_end = data + width * height; data != data_end; ++data)
    {
        RGBQUAD* from = reinterpret_cast<RGBQUAD*>(data);
        RGBQUAD* to = reinterpret_cast<RGBQUAD*>(data);

        to->rgbBlue  = (static_cast<uint16_t>(from->rgbBlue)  * from->rgbReserved) / 0xFF;
        to->rgbGreen = (static_cast<uint16_t>(from->rgbGreen) * from->rgbReserved) / 0xFF;
        to->rgbRed   = (static_cast<uint16_t>(from->rgbRed)   * from->rgbReserved) / 0xFF;
    }
}

// Converts an HCURSOR into a |MouseCursor| instance.
std::unique_ptr<MouseCursor> CreateMouseCursorFromHCursor(HDC dc, HCURSOR cursor)
{
    ICONINFO icon_info = { 0 };

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

    std::unique_ptr<uint32_t[]> mask_data =
        std::make_unique<uint32_t[]>(width * height);

    // Get pixel data from |scoped_mask| converting it to 32bpp along the way.
    // GetDIBits() sets the alpha component of every pixel to 0.
    BITMAPV5HEADER bmi = { 0 };

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
                   height,
                   mask_data.get(),
                   reinterpret_cast<BITMAPINFO*>(&bmi),
                   DIB_RGB_COLORS))
    {
        PLOG(LS_ERROR) << "GetDIBits failed";
        return nullptr;
    }

    uint32_t* mask_plane = mask_data.get();

    std::unique_ptr<uint8_t[]> image;
    size_t image_size;

    bool has_alpha = false;

    if (is_color)
    {
        image_size = width * height * kBytesPerPixel;
        image = std::make_unique<uint8_t[]>(image_size);

        // Get the pixels from the color bitmap.
        if (!GetDIBits(dc,
                       scoped_color,
                       0,
                       height,
                       image.get(),
                       reinterpret_cast<BITMAPINFO*>(&bmi),
                       DIB_RGB_COLORS))
        {
            PLOG(LS_ERROR) << "GetDIBits failed";
            return nullptr;
        }

        // GetDIBits() does not provide any indication whether the bitmap has
        // alpha channel, so we use HasAlphaChannel() below to find it out.
        has_alpha = HasAlphaChannel(reinterpret_cast<const uint32_t*>(image.get()), width, height);
    }
    else
    {
        // For non-color cursors, the mask contains both an AND and an XOR mask
        // and the height includes both. Thus, the width is correct, but we
        // need to divide by 2 to get the correct mask height.
        height /= 2;

        image_size = width * height * kBytesPerPixel;
        image = std::make_unique<uint8_t[]>(image_size);

        // The XOR mask becomes the color bitmap.
        memcpy(image.get(), mask_plane + (width * height), image_size);
    }

    //
    // Reconstruct transparency from the mask if the color image does not has
    // alpha channel.
    //
    if (!has_alpha)
    {
        bool add_outline = false;
        uint32_t* dst = reinterpret_cast<uint32_t*>(image.get());
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
            AddCursorOutline(width, height, reinterpret_cast<uint32_t*>(image.get()));
        }
    }

    // Pre-multiply the resulting pixels since MouseCursor uses premultiplied
    // images.
    AlphaMul(reinterpret_cast<uint32_t*>(image.get()), width, height);

    return MouseCursor::Create(std::move(image),
                               QSize(width, height),
                               QPoint(icon_info.xHotspot, icon_info.yHotspot));
}

bool IsSameCursorShape(const CURSORINFO& left, const CURSORINFO& right)
{
    // If the cursors are not showing, we do not care the hCursor handle.
    return left.flags == right.flags && (left.flags != CURSOR_SHOWING ||
                                         left.hCursor == right.hCursor);
}

} // namespace

CapturerGDI::CapturerGDI()
{
    memset(&prev_cursor_info_, 0, sizeof(prev_cursor_info_));
}

// static
std::unique_ptr<CapturerGDI> CapturerGDI::Create()
{
    return std::unique_ptr<CapturerGDI>(new CapturerGDI());
}

bool CapturerGDI::PrepareCaptureResources()
{
    // Switch to the desktop receiving user input if different from the
    // current one.
    Desktop input_desktop(Desktop::GetInputDesktop());

    if (input_desktop.IsValid() && !desktop_.IsSame(input_desktop))
    {
        // Release GDI resources otherwise SetThreadDesktop will fail.
        desktop_dc_.reset();
        memory_dc_.Reset();

        // If SetThreadDesktop() fails, the thread is still assigned a desktop.
        // So we can continue capture screen bits, just from the wrong desktop.
        desktop_.SetThreadDesktop(std::move(input_desktop));
    }

    QRect screen_rect = QRect(GetSystemMetrics(SM_XVIRTUALSCREEN),
                              GetSystemMetrics(SM_YVIRTUALSCREEN),
                              GetSystemMetrics(SM_CXVIRTUALSCREEN),
                              GetSystemMetrics(SM_CYVIRTUALSCREEN));

    // If the display bounds have changed then recreate GDI resources.
    if (screen_rect != desktop_dc_rect_)
    {
        desktop_dc_.reset();
        memory_dc_.Reset();

        desktop_dc_rect_ = QRect();
    }

    if (!desktop_dc_)
    {
        DCHECK(!memory_dc_);

        // Vote to disable Aero composited desktop effects while capturing.
        // Windows will restore Aero automatically if the process exits.
        // This has no effect under Windows 8 or higher. See crbug.com/124018.
        DwmEnableComposition(DWM_EC_DISABLECOMPOSITION);

        // Create GDI device contexts to capture from the desktop into memory.
        desktop_dc_ = std::make_unique<ScopedGetDC>(nullptr);
        memory_dc_.Reset(CreateCompatibleDC(*desktop_dc_));
        if (!memory_dc_)
        {
            LOG(LS_ERROR) << "CreateCompatibleDC failed";
            return false;
        }

        desktop_dc_rect_ = screen_rect;

        for (int i = 0; i < kNumFrames; ++i)
        {
            frame_[i] = DesktopFrameDIB::Create(screen_rect.size(),
                                                PixelFormat::ARGB(),
                                                memory_dc_);
            if (!frame_[i])
                return false;
        }

        differ_ = std::make_unique<Differ>(screen_rect.size());
    }

    return true;
}

const DesktopFrame* CapturerGDI::CaptureImage()
{
    if (!PrepareCaptureResources())
        return nullptr;

    int prev_frame_id = curr_frame_id_ - 1;
    if (prev_frame_id < 0)
        prev_frame_id = kNumFrames - 1;

    DesktopFrameDIB* prev_frame = frame_[prev_frame_id].get();
    DesktopFrameDIB* curr_frame = frame_[curr_frame_id_].get();

    {
        ScopedSelectObject select_object(memory_dc_, curr_frame->Bitmap());

        BitBlt(memory_dc_,
               0, 0,
               curr_frame->size().width(),
               curr_frame->size().height(),
               *desktop_dc_,
               desktop_dc_rect_.x(),
               desktop_dc_rect_.y(),
               CAPTUREBLT | SRCCOPY);
    }

    differ_->CalcDirtyRegion(prev_frame->GetFrameData(),
                             curr_frame->GetFrameData(),
                             curr_frame->MutableUpdatedRegion());

    curr_frame_id_ = prev_frame_id;

    return curr_frame;
}

std::unique_ptr<MouseCursor> CapturerGDI::CaptureCursor()
{
    CURSORINFO cursor_info = { 0 };

    // Note: cursor_info.hCursor does not need to be freed.
    cursor_info.cbSize = sizeof(cursor_info);
    if (GetCursorInfo(&cursor_info))
    {
        if (!IsSameCursorShape(cursor_info, prev_cursor_info_))
        {
            if (cursor_info.flags == 0)
            {
                // Host machine does not have a hardware mouse attached, we
                // will send a default one instead.
                // Note, Windows automatically caches cursor resource, so we
                // do not need to cache the result of LoadCursor.
                cursor_info.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            }

            std::unique_ptr<MouseCursor> mouse_cursor =
                CreateMouseCursorFromHCursor(*desktop_dc_, cursor_info.hCursor);

            if (mouse_cursor)
            {
                prev_cursor_info_ = cursor_info;
            }

            return mouse_cursor;
        }
    }

    return nullptr;
}

} // namespace aspia
