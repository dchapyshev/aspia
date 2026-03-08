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

#include "base/desktop/screen_capturer_win.h"

#include <QTimer>

#include "base/logging.h"
#include "base/desktop/screen_capturer_dxgi.h"
#include "base/desktop/screen_capturer_gdi.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/session_info.h"
#include "base/win/windows_version.h"

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
ScreenCapturer::ScreenType screenType(const wchar_t* desktop_name)
{
    if (_wcsicmp(desktop_name, L"winlogon") != 0)
        return ScreenCapturer::ScreenType::DESKTOP;

    DWORD session_id = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
    {
        PLOG(ERROR) << "ProcessIdToSessionId failed";
        return ScreenCapturer::ScreenType::UNKNOWN;
    }

    base::SessionInfo session_info(session_id);
    if (!session_info.isValid())
    {
        LOG(ERROR) << "Unable to get session info";
        return ScreenCapturer::ScreenType::UNKNOWN;
    }

    if (session_info.connectState() == base::SessionInfo::ConnectState::ACTIVE)
    {
        if (session_info.isUserLocked())
        {
            // Lock screen captured.
            return ScreenCapturer::ScreenType::LOCK;
        }
        else
        {
            // UAC screen captured.
            return ScreenCapturer::ScreenType::OTHER;
        }
    }

    return ScreenCapturer::ScreenType::LOGIN;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScreenCapturerWin::ScreenCapturerWin(Type type, QObject* parent)
    : ScreenCapturer(type, parent)
{
    LOG(INFO) << "Ctor";

    QTimer::singleShot(0, this, [this]()
    {
        switchToInputDesktop();

        // If the monitor is turned off, this call will turn it on.
        if (!SetThreadExecutionState(ES_DISPLAY_REQUIRED))
        {
            PLOG(ERROR) << "SetThreadExecutionState failed";
        }

        wchar_t desktop[100] = { 0 };
        if (desktop_.assignedDesktop().name(desktop, sizeof(desktop)))
        {
            if (_wcsicmp(desktop, L"Screen-saver") == 0)
            {
                LOG(INFO) << "Screen-saver desktop detected";

                auto send_key = [](WORD key_code, DWORD flags)
                {
                    INPUT input;
                    memset(&input, 0, sizeof(input));

                    input.type       = INPUT_KEYBOARD;
                    input.ki.wVk     = key_code;
                    input.ki.dwFlags = flags;
                    input.ki.wScan   = static_cast<WORD>(MapVirtualKeyW(key_code, MAPVK_VK_TO_VSC));

                    // Do the keyboard event.
                    if (!SendInput(1, &input, sizeof(input)))
                        PLOG(ERROR) << "SendInput failed";
                };

                send_key(VK_SPACE, 0);
                send_key(VK_SPACE, KEYEVENTF_KEYUP);
            }
        }
        else
        {
            LOG(ERROR) << "Unable to get name of desktop";
        }

        LOG(INFO) << "Checking current screen type";

        Desktop input_desktop = Desktop::inputDesktop();
        if (!input_desktop.isValid())
        {
            LOG(ERROR) << "Unable to get input desktop";
            return;
        }

        wchar_t desktop_name[128] = { 0 };
        if (!input_desktop.name(desktop_name, sizeof(desktop_name)))
        {
            LOG(ERROR) << "Unable to get desktop name";
            return;
        }

        checkScreenType(desktop_name);
    });
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerWin::~ScreenCapturerWin()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer* ScreenCapturerWin::create(Type preferred_type, Error last_error, QObject* parent)
{
    ScreenCapturer* screen_capturer = nullptr;

    if (last_error == ScreenCapturer::Error::PERMANENT)
    {
        LOG(INFO) << "Permanent error. Reset to GDI capturer";
    }
    else if (preferred_type == ScreenCapturer::Type::WIN_DXGI ||
             preferred_type == ScreenCapturer::Type::DEFAULT)
    {
        if (windowsVersion() >= VERSION_WIN8)
        {
            // Desktop Duplication API is available in Windows 8+.
            std::unique_ptr<ScreenCapturerDxgi> capturer_dxgi =
                std::make_unique<ScreenCapturerDxgi>();
            if (capturer_dxgi->isSupported())
            {
                LOG(INFO) << "Using DXGI capturer";
                screen_capturer = capturer_dxgi.release();
            }
        }
    }

    if (!screen_capturer)
    {
        LOG(INFO) << "Using GDI capturer";
        screen_capturer = new ScreenCapturerGdi();
    }

    screen_capturer->setParent(parent);
    return screen_capturer;
}

//--------------------------------------------------------------------------------------------------
// static
MouseCursor* ScreenCapturerWin::mouseCursorFromHCursor(HDC dc, HCURSOR cursor)
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
void ScreenCapturerWin::switchToInputDesktop()
{
    // Switch to the desktop receiving user input if different from the current one.
    Desktop input_desktop(Desktop::inputDesktop());

    if (!input_desktop.isValid() || desktop_.isSame(input_desktop))
        return;

    wchar_t new_name[128] = { 0 };
    input_desktop.name(new_name, sizeof(new_name));

    wchar_t old_name[128] = { 0 };
    desktop_.assignedDesktop().name(old_name, sizeof(old_name));

    LOG(INFO) << "Input desktop changed from" << old_name << "to" << new_name;

    reset();

    // If setThreadDesktop() fails, the thread is still assigned a desktop.
    // So we can continue capture screen bits, just from the wrong desktop.
    desktop_.setThreadDesktop(std::move(input_desktop));

    emit sig_desktopChanged();
    checkScreenType(new_name);
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWin::checkScreenType(const wchar_t* desktop_name)
{
    ScreenType screen_type = screenType(desktop_name);
    if (screen_type != last_screen_type_)
    {
        LOG(INFO) << "Screen type changed from" << last_screen_type_ << "to" << screen_type;

        QString screen_name = QString::fromWCharArray(desktop_name);
        if (screen_name.isEmpty())
            screen_name = "unknown";

        emit sig_screenTypeChanged(screen_type, screen_name);
        last_screen_type_ = screen_type;
    }
    else
    {
        LOG(INFO) << "Screen type not changed:" << last_screen_type_;
    }
}

} // namespace base
