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

#include "base/desktop/screen_capturer_gdi.h"

#include "base/logging.h"
#include "base/desktop/mouse_cursor.h"
#include "base/desktop/win/screen_capture_utils.h"
#include "base/desktop/frame_dib.h"
#include "base/desktop/differ.h"
#include "base/win/scoped_select_object.h"

#include <dwmapi.h>

namespace base {

namespace {

constexpr quint32 RGBA(quint32 r, quint32 g, quint32 b, quint32 a)
{
    return (((a << 24) & 0xFF000000) | ((b << 16) & 0xFF0000) | ((g << 8) & 0xFF00) | (r & 0xFF));
}

constexpr int kBytesPerPixel = 4;

// Pixel colors used when generating cursor outlines.
constexpr quint32 kPixelRgbaBlack       = RGBA(0,    0,    0,    0xFF);
constexpr quint32 kPixelRgbaWhite       = RGBA(0xFF, 0xFF, 0xFF, 0xFF);
constexpr quint32 kPixelRgbaTransparent = RGBA(0,    0,    0,    0);

constexpr quint32 kPixelRgbWhite = RGB(0xFF, 0xFF, 0xFF);

//--------------------------------------------------------------------------------------------------
// Scans a 32bpp bitmap looking for any pixels with non-zero alpha component.
// Returns true if non-zero alpha is found. |stride| is expressed in pixels.
bool hasAlphaChannel(const quint32* data, int width, int height)
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
void addCursorOutline(int width, int height, quint32* data)
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
void alphaMul(quint32* data, int width, int height)
{
    static_assert(sizeof(quint32) == kBytesPerPixel,
                  "size of uint32 should be the number of bytes per pixel");

    for (quint32* data_end = data + width * height; data != data_end; ++data)
    {
        RGBQUAD* from = reinterpret_cast<RGBQUAD*>(data);
        RGBQUAD* to = reinterpret_cast<RGBQUAD*>(data);

        to->rgbBlue  = static_cast<BYTE>((static_cast<quint16>(from->rgbBlue)  * from->rgbReserved) / 0xFF);
        to->rgbGreen = static_cast<BYTE>((static_cast<quint16>(from->rgbGreen) * from->rgbReserved) / 0xFF);
        to->rgbRed   = static_cast<BYTE>((static_cast<quint16>(from->rgbRed)   * from->rgbReserved) / 0xFF);
    }
}

//--------------------------------------------------------------------------------------------------
bool isSameCursorShape(const CURSORINFO& left, const CURSORINFO& right)
{
    // If the cursors are not showing, we do not care the hCursor handle.
    return left.flags == right.flags && (left.flags != CURSOR_SHOWING ||
                                         left.hCursor == right.hCursor);
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScreenCapturerGdi::ScreenCapturerGdi(QObject* parent)
    : ScreenCapturerWin(Type::WIN_GDI, parent)
{
    LOG(INFO) << "Ctor";

    memset(&curr_cursor_info_, 0, sizeof(curr_cursor_info_));
    memset(&prev_cursor_info_, 0, sizeof(prev_cursor_info_));

    dwmapi_dll_ = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (dwmapi_dll_)
    {
        dwm_enable_composition_func_ = reinterpret_cast<DwmEnableCompositionFunc>(
            GetProcAddress(dwmapi_dll_, "DwmEnableComposition"));
        if (!dwm_enable_composition_func_)
        {
            PLOG(ERROR) << "Unable to load DwmEnableComposition function";
        }

        dwm_is_composition_enabled_func_ = reinterpret_cast<DwmIsCompositionEnabledFunc>(
            GetProcAddress(dwmapi_dll_, "DwmIsCompositionEnabled"));
        if (!dwm_is_composition_enabled_func_)
        {
            PLOG(ERROR) << "Unable to load DwmIsCompositionEnabled function";
        }
    }
    else
    {
        PLOG(ERROR) << "Unable to load dwmapi.dll";
    }
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerGdi::~ScreenCapturerGdi()
{
    LOG(INFO) << "Dtor";

    if (composition_changed_ && dwm_enable_composition_func_)
        dwm_enable_composition_func_(DWM_EC_ENABLECOMPOSITION);

    if (dwmapi_dll_)
        FreeLibrary(dwmapi_dll_);
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerGdi::screenCount()
{
    return ScreenCaptureUtils::screenCount();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerGdi::screenList(ScreenList* screens)
{
    return ScreenCaptureUtils::screenList(screens);
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerGdi::selectScreen(ScreenId screen_id)
{
    LOG(INFO) << "Select screen with ID:" << screen_id;

    if (!ScreenCaptureUtils::isScreenValid(screen_id, &current_device_key_))
    {
        LOG(ERROR) << "Invalid screen";
        return false;
    }

    // At next screen capture, the resources are recreated.
    desktop_dc_rect_ = QRect();

    current_screen_id_ = screen_id;
    return true;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerGdi::currentScreen() const
{
    return current_screen_id_;
}

//--------------------------------------------------------------------------------------------------
const Frame* ScreenCapturerGdi::captureFrame(Error* error)
{
    DCHECK(error);

    queue_.moveToNextFrame();
    *error = Error::TEMPORARY;

    if (!prepareCaptureResources())
        return nullptr;

    screen_rect_ = ScreenCaptureUtils::screenRect(current_screen_id_, current_device_key_);
    if (screen_rect_.isEmpty())
    {
        LOG(ERROR) << "Failed to get screen rect";
        *error = Error::PERMANENT;
        return nullptr;
    }

    if (!queue_.currentFrame() || queue_.currentFrame()->size() != screen_rect_.size())
    {
        DCHECK(desktop_dc_);
        DCHECK(memory_dc_);

        std::unique_ptr<Frame> frame = FrameDib::create(
            screen_rect_.size(), PixelFormat::ARGB(), memory_dc_);
        if (!frame)
        {
            LOG(ERROR) << "Failed to create frame buffer";
            return nullptr;
        }

        frame->setCapturerType(static_cast<quint32>(type()));
        queue_.replaceCurrentFrame(std::move(frame));
    }

    Frame* current = queue_.currentFrame();
    Frame* previous = queue_.previousFrame();

    {
        ScopedSelectObject select_object(
            memory_dc_, static_cast<FrameDib*>(current)->bitmap());

        if (!BitBlt(memory_dc_,
                    0, 0,
                    screen_rect_.width(), screen_rect_.height(),
                    desktop_dc_,
                    screen_rect_.left(), screen_rect_.top(),
                    CAPTUREBLT | SRCCOPY))
        {
            static thread_local int count = 0;

            if (count == 0)
            {
                LOG(ERROR) << "BitBlt failed";
            }

            if (++count > 10)
                count = 0;

            return nullptr;
        }
    }

    current->setTopLeft(screen_rect_.topLeft() - desktop_dc_rect_.topLeft());

    if (!previous || previous->size() != current->size())
    {
        differ_ = std::make_unique<Differ>(screen_rect_.size());
        *current->updatedRegion() += QRect(QPoint(0, 0), screen_rect_.size());
    }
    else
    {
        differ_->calcDirtyRegion(previous->frameData(),
                                 current->frameData(),
                                 current->updatedRegion());
    }

    *error = Error::SUCCEEDED;
    return current;
}

//--------------------------------------------------------------------------------------------------
const MouseCursor* ScreenCapturerGdi::captureCursor()
{
    if (!desktop_dc_.isValid())
        return nullptr;

    memset(&curr_cursor_info_, 0, sizeof(curr_cursor_info_));

    // Note: cursor_info.hCursor does not need to be freed.
    curr_cursor_info_.cbSize = sizeof(curr_cursor_info_);
    if (GetCursorInfo(&curr_cursor_info_))
    {
        if (!isSameCursorShape(curr_cursor_info_, prev_cursor_info_))
        {
            if (curr_cursor_info_.flags == 0)
            {
                LOG(INFO) << "No hardware cursor attached. Using default mouse cursor";

                // Host machine does not have a hardware mouse attached, we will send a default one
                // instead. Note, Windows automatically caches cursor resource, so we do not need
                // to cache the result of LoadCursor.
                curr_cursor_info_.hCursor = LoadCursorW(nullptr, IDC_ARROW);
                if (!curr_cursor_info_.hCursor)
                {
                    PLOG(ERROR) << "LoadCursorW failed";
                    return nullptr;
                }
            }

            mouse_cursor_.reset(mouseCursorFromHCursor(desktop_dc_, curr_cursor_info_.hCursor));
            if (mouse_cursor_)
            {
                prev_cursor_info_ = curr_cursor_info_;

                int dpi_x = GetDeviceCaps(desktop_dc_, LOGPIXELSX);
                int dpi_y = GetDeviceCaps(desktop_dc_, LOGPIXELSY);

                mouse_cursor_->dpi() = QPoint(dpi_x, dpi_y);
                return mouse_cursor_.get();
            }
        }
    }
    else
    {
        PLOG(ERROR) << "GetCursorInfo failed";
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
QPoint ScreenCapturerGdi::cursorPosition()
{
    QPoint cursor_pos(curr_cursor_info_.ptScreenPos.x, curr_cursor_info_.ptScreenPos.y);

    if (current_screen_id_ == kFullDesktopScreenId)
        cursor_pos = cursor_pos - desktop_dc_rect_.topLeft();
    else
        cursor_pos = cursor_pos - screen_rect_.topLeft();

    return cursor_pos;
}

//--------------------------------------------------------------------------------------------------
// static
MouseCursor* ScreenCapturerGdi::mouseCursorFromHCursor(HDC dc, HCURSOR cursor)
{
    ICONINFO icon_info;
    memset(&icon_info, 0, sizeof(icon_info));

    if (!GetIconInfo(cursor, &icon_info))
    {
        PLOG(ERROR) << "GetIconInfo failed";
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
        PLOG(ERROR) << "GetObjectW failed";
        return nullptr;
    }

    int width = bitmap_info.bmWidth;
    int height = bitmap_info.bmHeight;

    QVector<quint32> mask_data(static_cast<size_t>(width) * static_cast<size_t>(height));

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

    if (!GetDIBits(dc, scoped_mask, 0, static_cast<UINT>(height), mask_data.data(),
        reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS))
    {
        PLOG(ERROR) << "GetDIBits failed";
        return nullptr;
    }

    quint32* mask_plane = mask_data.data();
    QByteArray image;
    bool has_alpha = false;

    if (is_color)
    {
        image.resize(static_cast<size_t>(width) * static_cast<size_t>(height) *
                     static_cast<size_t>(kBytesPerPixel));

        // Get the pixels from the color bitmap.
        if (!GetDIBits(dc, scoped_color, 0, static_cast<UINT>(height), image.data(),
            reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS))
        {
            PLOG(ERROR) << "GetDIBits failed";
            return nullptr;
        }

        // GetDIBits() does not provide any indication whether the bitmap has
        // alpha channel, so we use hasAlphaChannel() below to find it out.
        has_alpha = hasAlphaChannel(reinterpret_cast<const quint32*>(image.data()), width, height);
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
        quint32* dst = reinterpret_cast<quint32*>(image.data());
        quint32* mask = mask_plane;

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
            addCursorOutline(width, height, reinterpret_cast<quint32*>(image.data()));
        }
    }

    // Pre-multiply the resulting pixels since MouseCursor uses premultiplied images.
    alphaMul(reinterpret_cast<quint32*>(image.data()), width, height);

    return new MouseCursor(std::move(image),
                           QSize(width, height),
                           QPoint(static_cast<int>(icon_info.xHotspot),
                                  static_cast<int>(icon_info.yHotspot)));
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerGdi::reset()
{
    // Release GDI resources otherwise SetThreadDesktop will fail.
    desktop_dc_.close();
    memory_dc_.reset();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerGdi::prepareCaptureResources()
{
    QRect desktop_rect = ScreenCaptureUtils::fullScreenRect();

    // If the display bounds have changed then recreate GDI resources.
    if (desktop_rect != desktop_dc_rect_)
    {
        LOG(INFO) << "Desktop rect changed from" << desktop_dc_rect_ << "to" << desktop_rect;

        desktop_dc_.close();
        memory_dc_.reset();

        desktop_dc_rect_ = QRect();
    }

    if (!desktop_dc_)
    {
        DCHECK(!memory_dc_);

        if (dwm_enable_composition_func_ && dwm_is_composition_enabled_func_)
        {
            BOOL enabled;
            HRESULT hr = dwm_is_composition_enabled_func_(&enabled);
            if (SUCCEEDED(hr) && enabled)
            {
                // Vote to disable Aero composited desktop effects while capturing.
                // Windows will restore Aero automatically if the process exits.
                // This has no effect under Windows 8 or higher.
                dwm_enable_composition_func_(DWM_EC_DISABLECOMPOSITION);
                composition_changed_ = true;
            }
        }

        // Create GDI device contexts to capture from the desktop into memory.
        desktop_dc_.getDC(nullptr);
        memory_dc_.reset(CreateCompatibleDC(desktop_dc_));
        if (!memory_dc_)
        {
            LOG(ERROR) << "CreateCompatibleDC failed";
            return false;
        }

        desktop_dc_rect_ = desktop_rect;

        // Make sure the frame buffers will be reallocated.
        queue_.reset();
    }

    return true;
}

} // namespace base
