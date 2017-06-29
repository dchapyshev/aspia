//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/scoped_begin_paint.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__SCOPED_BEGIN_PAINT_H
#define _ASPIA_UI__BASE__SCOPED_BEGIN_PAINT_H

#include "base/macros.h"
#include "base/logging.h"

namespace aspia {

class ScopedBeginPaint
{
public:
    ScopedBeginPaint(HWND hwnd) :
        hwnd_(hwnd)
    {
        paint_dc_ = BeginPaint(hwnd_, &ps_);
        DCHECK(paint_dc_);
    }

    ~ScopedBeginPaint()
    {
        if (paint_dc_)
        {
            EndPaint(hwnd_, &ps_);
        }
    }

    HDC DC() const
    {
        return paint_dc_;
    }

    UiRect Rect() const
    {
        return UiRect(ps_.rcPaint);
    }

private:
    PAINTSTRUCT ps_;
    HDC paint_dc_;
    HWND hwnd_;

    DISALLOW_COPY_AND_ASSIGN(ScopedBeginPaint);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__SCOPED_BEGIN_PAINT_H
