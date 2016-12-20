/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/capturer_gdi.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "desktop_capture/capturer_gdi.h"

#include "base/logging.h"
#include "base/scoped_select_object.h"

namespace aspia {

CapturerGDI::CapturerGDI() :
    curr_buffer_id_(0),
    desktop_effects_(new ScopedDesktopEffects())
{
    memset(image_buffer_, 0, sizeof(image_buffer_));
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

    format.SetBitsPerPixel(bmi.header.biBitCount);

    format.SetRedShift(red_shift);
    format.SetGreenShift(green_shift);
    format.SetBlueShift(blue_shift);

    format.SetRedMax(bmi.u.mask.red >> format.RedShift());
    format.SetGreenMax(bmi.u.mask.green >> format.GreenShift());
    format.SetBlueMax(bmi.u.mask.blue >> format.BlueShift());

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

    bmi.header.biSizeImage = current_pixel_format_.BytesPerPixel() *
        aligned_width * current_desktop_rect_.height();

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

        current_desktop_rect_ = desktop_rect;
    }

    if (!desktop_dc_)
    {
        DCHECK(!memory_dc_);

        desktop_dc_.reset(new ScopedGetDC(nullptr));
        memory_dc_.set(CreateCompatibleDC(*desktop_dc_));

        for (int index = 0; index < kNumBuffers; ++index)
        {
            AllocateBuffer(index, 32);
        }

        differ_.reset(new Differ(current_desktop_rect_.size(),
                                 current_pixel_format_.BytesPerPixel()));
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
    while (true)
    {
        PrepareCaptureResources();

        desktop_size = current_desktop_rect_.size();
        pixel_format = current_pixel_format_;

        int prev_buffer_id = curr_buffer_id_ - 1;
        if (prev_buffer_id < 0)
            prev_buffer_id = kNumBuffers - 1;

        ScopedSelectObject select_object(memory_dc_,
                                         target_bitmap_[curr_buffer_id_]);

        if (!BitBlt(memory_dc_,
                    0, 0,
                    current_desktop_rect_.width(),
                    current_desktop_rect_.height(),
                    *desktop_dc_,
                    current_desktop_rect_.x(),
                    current_desktop_rect_.y(),
                    CAPTUREBLT | SRCCOPY))
        {
            PrepareInputDesktop();
            continue;
        }

        const uint8_t *prev = image_buffer_[prev_buffer_id];
        const uint8_t *curr = image_buffer_[curr_buffer_id_];

        differ_->CalcChangedRegion(prev, curr, changed_region);

        curr_buffer_id_ = prev_buffer_id;

        return curr;
    }
}

} // namespace aspia
