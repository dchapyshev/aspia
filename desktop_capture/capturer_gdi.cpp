//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/capturer_gdi.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/capturer_gdi.h"

#include "base/exception.h"
#include "base/logging.h"
#include "base/scoped_select_object.h"

#include "desktop_capture/cursor.h"

namespace aspia {

static const int kBytesPerPixel = 4;

CapturerGDI::CapturerGDI() :
    curr_buffer_id_(0)
{
    image_buffer_[0] = nullptr;
    image_buffer_[1] = nullptr;

    memset(&prev_cursor_info_, 0, sizeof(prev_cursor_info_));
}

void CapturerGDI::AllocateBuffer(int buffer_index, int align)
{
    DCHECK(desktop_dc_);
    DCHECK(memory_dc_);

    int aligned_width = ((screen_rect_.Width() + (align - 1)) / align) * 2;

    BitmapInfo bmi = { 0 };

    bmi.header.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.header.biBitCount    = kBytesPerPixel * 8;
    bmi.header.biCompression = BI_BITFIELDS;
    bmi.header.biSizeImage   = kBytesPerPixel * aligned_width * screen_rect_.Height();
    bmi.header.biPlanes      = 1;
    bmi.header.biWidth       = screen_rect_.Width();
    bmi.header.biHeight      = -screen_rect_.Height();

    // 0RGB (alpha = 0)
    bmi.u.mask.red   = 0x00FF0000;
    bmi.u.mask.green = 0x0000FF00;
    bmi.u.mask.blue  = 0x000000FF;

    target_bitmap_[buffer_index] =
        CreateDIBSection(memory_dc_,
                         reinterpret_cast<PBITMAPINFO>(&bmi),
                         DIB_RGB_COLORS,
                         reinterpret_cast<void**>(&image_buffer_[buffer_index]),
                         nullptr,
                         0);
    if (!target_bitmap_[buffer_index].Get())
    {
        LOG(ERROR) << "CreateDIBSection() failed: " << GetLastError();
    }
}

void CapturerGDI::PrepareCaptureResources()
{
    DesktopRect screen_rect =
        DesktopRect::MakeXYWH(GetSystemMetrics(SM_XVIRTUALSCREEN),
                              GetSystemMetrics(SM_YVIRTUALSCREEN),
                              GetSystemMetrics(SM_CXVIRTUALSCREEN),
                              GetSystemMetrics(SM_CYVIRTUALSCREEN));

    if (screen_rect_ != screen_rect)
    {
        differ_.reset();
        desktop_dc_.reset();
        memory_dc_.set(nullptr);

        screen_rect_ = screen_rect;
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

        differ_.reset(new Differ(screen_rect_.Size(), 16));
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

const uint8_t* CapturerGDI::CaptureImage(DesktopRegion *dirty_region,
                                         DesktopSize *desktop_size)
{
    while (true)
    {
        PrepareCaptureResources();

        *desktop_size = screen_rect_.Size();

        int prev_buffer_id = curr_buffer_id_ - 1;
        if (prev_buffer_id < 0)
            prev_buffer_id = kNumBuffers - 1;

        {
            ScopedSelectObject select_object(memory_dc_,
                                             target_bitmap_[curr_buffer_id_]);

            if (!BitBlt(memory_dc_,
                        0, 0,
                        screen_rect_.Width(),
                        screen_rect_.Height(),
                        *desktop_dc_,
                        screen_rect_.x(),
                        screen_rect_.y(),
                        CAPTUREBLT | SRCCOPY))
            {
                PrepareInputDesktop();
                continue;
            }
        }

        const uint8_t *prev = image_buffer_[prev_buffer_id];
        const uint8_t *curr = image_buffer_[curr_buffer_id_];

        differ_->CalcDirtyRegion(prev, curr, dirty_region);

        curr_buffer_id_ = prev_buffer_id;

        return curr;
    }
}

static bool IsSameCursorShape(const CURSORINFO &left, const CURSORINFO &right)
{
    // If the cursors are not showing, we do not care the hCursor handle.
    return left.flags == right.flags && (left.flags != CURSOR_SHOWING ||
                                         left.hCursor == right.hCursor);
}

MouseCursor* CapturerGDI::CaptureCursor()
{
    CURSORINFO cursor_info = { 0 };

    // Note: cursor_info.hCursor does not need to be freed.
    cursor_info.cbSize = sizeof(CURSORINFO);
    if (!GetCursorInfo(&cursor_info))
    {
        LOG(ERROR) << "GetCursorInfo() failed: " << GetLastError();
        throw Exception("Unable to retrieve the cursor image");
    }

    if (!IsSameCursorShape(cursor_info, prev_cursor_info_))
    {
        if (cursor_info.flags == 0)
        {
            //
            // Host machine does not have a hardware mouse attached, we will send a
            // default one instead.
            // Note, Windows automatically caches cursor resource, so we do not need
            // to cache the result of LoadCursor.
            //
            cursor_info.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        }

        MouseCursor *mouse_cursor = CreateMouseCursorFromHCursor(*desktop_dc_, cursor_info.hCursor);

        if (mouse_cursor)
        {
            prev_cursor_info_ = cursor_info;
        }

        return mouse_cursor;
    }

    return nullptr;
}

} // namespace aspia
