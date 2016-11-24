/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/capturer_gdi.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "desktop_capture/capturer_gdi.h"

#include "base/logging.h"

CapturerGDI::CapturerGDI() :
    curr_buffer_id_(0)
{
    memset(image_buffer_, 0, sizeof(image_buffer_));

    desktop_effects_.reset(new DesktopEffects());
}

void CapturerGDI::GetDefaultBitmapInfo(BitmapInfo *bmi)
{
    DEVMODEW mode = { 0 };

    mode.dmSize = sizeof(DEVMODEW);
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode))
    {
        if (mode.dmBitsPerPel != 16 && mode.dmBitsPerPel != 32)
        {
            mode.dmBitsPerPel = 32;
        }
    }
    else
    {
        mode.dmBitsPerPel = 32;
    }

    memset(bmi, 0, sizeof(BitmapInfo));

    bmi->header.biSize        = sizeof(BITMAPINFOHEADER);
    bmi->header.biBitCount    = static_cast<WORD>(mode.dmBitsPerPel);
    bmi->header.biCompression = BI_BITFIELDS;

    if (mode.dmBitsPerPel == 32)
    {
        // 0RGB (alpha = 0)
        bmi->u.mask.red   = 0x00FF0000;
        bmi->u.mask.green = 0x0000FF00;
        bmi->u.mask.blue  = 0x000000FF;
    }
    else if (mode.dmBitsPerPel == 16)
    {
        // RGB565
        bmi->u.mask.red   = 0xF800;
        bmi->u.mask.green = 0x07E0;
        bmi->u.mask.blue  = 0x001F;
    }
}

PixelFormat CapturerGDI::GetPixelFormat(const BitmapInfo &bmi)
{
    if (bmi.header.biCompression != BI_BITFIELDS)
        return PixelFormat::MakeARGB();

    int red_shift;
    int green_shift;
    int blue_shift;
    uint32_t bits;

    for (bits = bmi.u.mask.red, red_shift = 0;
         (red_shift < 32) && ((bits & 1) == 0);
         ++red_shift)
    {
        bits >>= 1;
    }

    for (bits = bmi.u.mask.green, green_shift = 0;
         (green_shift < 32) && ((bits & 1) == 0);
         ++green_shift)
    {
        bits >>= 1;
    }

    for (bits = bmi.u.mask.blue, blue_shift = 0;
         (blue_shift < 32) && ((bits & 1) == 0);
         ++blue_shift)
    {
        bits >>= 1;
    }

    PixelFormat format;

    format.set_bits_per_pixel(bmi.header.biBitCount);

    format.set_red_shift(red_shift);
    format.set_green_shift(green_shift);
    format.set_blue_shift(blue_shift);

    format.set_red_max(bmi.u.mask.red     >> format.red_shift());
    format.set_green_max(bmi.u.mask.green >> format.green_shift());
    format.set_blue_max(bmi.u.mask.blue   >> format.blue_shift());

    return format;
}

void CapturerGDI::AllocateBuffer(int buffer_index, int align)
{
    DCHECK(desktop_dc_);
    DCHECK(memory_dc_);

    int aligned_width = ((current_desktop_rect_.width() + (align - 1)) / align) * 2;

    BitmapInfo bmi;
    GetDefaultBitmapInfo(&bmi);

    current_pixel_format_ = GetPixelFormat(bmi);

    bmi.header.biSizeImage = current_pixel_format_.bytes_per_pixel() * aligned_width * current_desktop_rect_.height();
    bmi.header.biPlanes    = 1;
    bmi.header.biWidth     = current_desktop_rect_.width();
    bmi.header.biHeight    = -current_desktop_rect_.height();

    target_bitmap_[buffer_index] =
        CreateDIBSection(memory_dc_,
                         reinterpret_cast<PBITMAPINFO>(&bmi),
                         DIB_RGB_COLORS,
                         reinterpret_cast<void**>(&image_buffer_[buffer_index]),
                         nullptr,
                         0);

    DLOG(INFO) << "buffer[" << buffer_index << "] initialized.";
}

// static
DesktopRect CapturerGDI::GetDesktopRect()
{
    return DesktopRect::MakeXYWH(GetSystemMetrics(SM_XVIRTUALSCREEN),
                                 GetSystemMetrics(SM_YVIRTUALSCREEN),
                                 GetSystemMetrics(SM_CXVIRTUALSCREEN),
                                 GetSystemMetrics(SM_CYVIRTUALSCREEN));
}

void CapturerGDI::PrepareCaptureResources()
{
    DesktopRect desktop_rect = GetDesktopRect();

    if (current_desktop_rect_ != desktop_rect)
    {
        differ_.reset();
        desktop_dc_.reset();
        memory_dc_.set(nullptr);

        DLOG(INFO) << "desktop rect (x:y:w:h) changed from "
            << current_desktop_rect_.x() << ":" << current_desktop_rect_.y() << ":"
            << current_desktop_rect_.width() << ":" << current_desktop_rect_.height()
            << " to "
            << desktop_rect.x() << ":" << desktop_rect.y() << ":"
            << desktop_rect.width() << ":" << desktop_rect.height();

        current_desktop_rect_ = desktop_rect;
    }

    if (!desktop_dc_)
    {
        DCHECK(!memory_dc_);

        desktop_dc_.reset(new ScopedGetDC(nullptr));
        memory_dc_.set(CreateCompatibleDC(*desktop_dc_));

        for (int index = 0; index < kNumBuffers; index++)
        {
            AllocateBuffer(index, 32);
        }
    }

    if (!differ_)
    {
        differ_.reset(new Differ(current_desktop_rect_.size(),
                                 current_pixel_format_.bytes_per_pixel()));
    }
}

void CapturerGDI::PrepareInputDesktop()
{
    std::unique_ptr<Desktop> input_desktop(Desktop::GetInputDesktop());

    if (input_desktop && !desktop_.IsSame(*input_desktop))
    {
        differ_.reset();
        desktop_dc_.reset();
        memory_dc_.set(nullptr);

        desktop_.SetThreadDesktop(input_desktop.release());
    }
}

const uint8_t* CapturerGDI::CaptureImage(DesktopRegion &changed_region,
                                         DesktopSize &desktop_size,
                                         PixelFormat &pixel_format)
{
Retry:
    PrepareCaptureResources();

    desktop_size = current_desktop_rect_.size();
    pixel_format = current_pixel_format_;

    int prev_buffer_id = curr_buffer_id_ - 1;
    if (prev_buffer_id < 0)
        prev_buffer_id = kNumBuffers - 1;

    HGDIOBJ prev_object = SelectObject(memory_dc_,
                                       target_bitmap_[curr_buffer_id_]);
    if (prev_object)
    {
        if (!BitBlt(memory_dc_,
                    0, 0,
                    current_desktop_rect_.width(), current_desktop_rect_.height(),
                    *desktop_dc_,
                    current_desktop_rect_.x(), current_desktop_rect_.y(),
                    CAPTUREBLT | SRCCOPY))
        {
            PrepareInputDesktop();
            SelectObject(memory_dc_, prev_object);
            goto Retry;
        }

        SelectObject(memory_dc_, prev_object);
    }

    const uint8_t *prev = image_buffer_[prev_buffer_id];
    const uint8_t *curr = image_buffer_[curr_buffer_id_];

    differ_->CalcChangedRegion(prev, curr, changed_region);

    curr_buffer_id_ = prev_buffer_id;

    return curr;
}
