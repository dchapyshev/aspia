//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/capturer_gdi.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/capturer_gdi.h"

#include "base/logging.h"
#include "base/scoped_select_object.h"

#include "desktop_capture/cursor.h"

namespace aspia {

CapturerGDI::CapturerGDI() :
    curr_frame_id_(0)
{
    memset(&prev_cursor_info_, 0, sizeof(prev_cursor_info_));
}

bool CapturerGDI::PrepareInputDesktop()
{
    Desktop input_desktop(Desktop::GetInputDesktop());

    if (input_desktop.IsValid() && !desktop_.IsSame(input_desktop))
    {
        desktop_dc_.reset();
        memory_dc_.set(nullptr);

        desktop_.SetThreadDesktop(std::move(input_desktop));

        return true;
    }

    return false;
}

void CapturerGDI::PrepareCaptureResources()
{
    DesktopRect screen_rect =
        DesktopRect::MakeXYWH(GetSystemMetrics(SM_XVIRTUALSCREEN),
                              GetSystemMetrics(SM_YVIRTUALSCREEN),
                              GetSystemMetrics(SM_CXVIRTUALSCREEN),
                              GetSystemMetrics(SM_CYVIRTUALSCREEN));

    if (!screen_rect_.IsEqual(screen_rect))
    {
        desktop_dc_.reset();
        memory_dc_.set(nullptr);

        screen_rect_ = screen_rect;
    }

    if (!desktop_dc_)
    {
        DCHECK(!memory_dc_);

        desktop_dc_.reset(new ScopedGetDC(nullptr));
        memory_dc_.set(CreateCompatibleDC(*desktop_dc_));

        DesktopSize size = screen_rect_.Size();

        for (int i = 0; i < kNumFrames; ++i)
        {
            frame_[i].reset(DesktopFrameDib::Create(size, PixelFormat::ARGB(), memory_dc_));
            CHECK(frame_[i]);
        }

        differ_.reset(new Differ(size));
    }
}

const DesktopFrame* CapturerGDI::CaptureImage(bool* desktop_change)
{
    *desktop_change = PrepareInputDesktop();
    PrepareCaptureResources();

    int prev_frame_id = curr_frame_id_ - 1;
    if (prev_frame_id < 0)
        prev_frame_id = kNumFrames - 1;

    DesktopFrameDib* prev_frame = frame_[prev_frame_id].get();
    DesktopFrameDib* curr_frame = frame_[curr_frame_id_].get();

    {
        ScopedSelectObject select_object(memory_dc_, curr_frame->Bitmap());

        if (!BitBlt(memory_dc_,
                    0, 0,
                    curr_frame->Size().Width(),
                    curr_frame->Size().Height(),
                    *desktop_dc_,
                    screen_rect_.x(),
                    screen_rect_.y(),
                    CAPTUREBLT | SRCCOPY))
        {
            DLOG(WARNING) << "BitBlt() failed: " << GetLastError();
            return prev_frame;
        }
    }

    // Если рабочий стол не был переключен.
    if (!*desktop_change)
    {
        // Вычисляем изменившиеся области.
        differ_->CalcDirtyRegion(prev_frame->GetFrameData(),
                                 curr_frame->GetFrameData(),
                                 curr_frame->MutableUpdatedRegion());
    }
    else
    {
        // Добавляем в измененный регион всю область экрана.
        curr_frame->InvalidateFrame();
    }

    curr_frame_id_ = prev_frame_id;

    return curr_frame;
}

static bool IsSameCursorShape(const CURSORINFO& left, const CURSORINFO& right)
{
    // If the cursors are not showing, we do not care the hCursor handle.
    return left.flags == right.flags && (left.flags != CURSOR_SHOWING ||
                                         left.hCursor == right.hCursor);
}

MouseCursor* CapturerGDI::CaptureCursor()
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
                //
                // Host machine does not have a hardware mouse attached, we will send a
                // default one instead.
                // Note, Windows automatically caches cursor resource, so we do not need
                // to cache the result of LoadCursor.
                //
                cursor_info.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            }

            MouseCursor* mouse_cursor =
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
