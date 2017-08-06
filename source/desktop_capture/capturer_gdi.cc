//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/capturer_gdi.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/capturer_gdi.h"
#include "base/logging.h"
#include "base/scoped_select_object.h"
#include "desktop_capture/cursor.h"

namespace aspia {

// Constants from dwmapi.h.
const UINT DWM_EC_DISABLECOMPOSITION = 0;
const UINT DWM_EC_ENABLECOMPOSITION = 1;

const wchar_t kDwmapiLibraryName[] = L"dwmapi.dll";

CapturerGDI::CapturerGDI()
    : dwmapi_library_(kDwmapiLibraryName)
{
    memset(&prev_cursor_info_, 0, sizeof(prev_cursor_info_));

    composition_func_ =
        reinterpret_cast<DwmEnableCompositionFunc>(
            dwmapi_library_.GetFunctionPointer("DwmEnableComposition"));
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
    std::unique_ptr<Desktop> input_desktop(Desktop::GetInputDesktop());

    if (input_desktop && !desktop_.IsSame(*input_desktop))
    {
        // Release GDI resources otherwise SetThreadDesktop will fail.
        desktop_dc_.reset();
        memory_dc_.Reset();

        // If SetThreadDesktop() fails, the thread is still assigned a desktop.
        // So we can continue capture screen bits, just from the wrong desktop.
        desktop_.SetThreadDesktop(input_desktop.release());
    }

    DesktopRect screen_rect =
        DesktopRect::MakeXYWH(GetSystemMetrics(SM_XVIRTUALSCREEN),
                              GetSystemMetrics(SM_YVIRTUALSCREEN),
                              GetSystemMetrics(SM_CXVIRTUALSCREEN),
                              GetSystemMetrics(SM_CYVIRTUALSCREEN));

    // If the display bounds have changed then recreate GDI resources.
    if (!screen_rect.IsEqual(desktop_dc_rect_))
    {
        desktop_dc_.reset();
        memory_dc_.Reset();

        desktop_dc_rect_ = DesktopRect();
    }

    if (!desktop_dc_)
    {
        DCHECK(!memory_dc_);

        // Vote to disable Aero composited desktop effects while capturing.
        // Windows will restore Aero automatically if the process exits.
        // This has no effect under Windows 8 or higher. See crbug.com/124018.
        if (composition_func_)
        {
            composition_func_(DWM_EC_DISABLECOMPOSITION);
        }

        // Create GDI device contexts to capture from the desktop into memory.
        desktop_dc_ = std::make_unique<ScopedGetDC>(nullptr);
        memory_dc_.Reset(CreateCompatibleDC(*desktop_dc_));
        if (!memory_dc_)
        {
            LOG(ERROR) << "CreateCompatibleDC() failed";
            return false;
        }

        desktop_dc_rect_ = screen_rect;

        for (int i = 0; i < kNumFrames; ++i)
        {
            frame_[i] = DesktopFrameDIB::Create(screen_rect.Size(),
                                                PixelFormat::ARGB(),
                                                memory_dc_);
            if (!frame_[i])
                return false;
        }

        differ_ = std::make_unique<Differ>(screen_rect.Size());
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
               curr_frame->Size().Width(),
               curr_frame->Size().Height(),
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

static bool IsSameCursorShape(const CURSORINFO& left, const CURSORINFO& right)
{
    // If the cursors are not showing, we do not care the hCursor handle.
    return left.flags == right.flags && (left.flags != CURSOR_SHOWING ||
                                         left.hCursor == right.hCursor);
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
