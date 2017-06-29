//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/static.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__STATIC_H
#define _ASPIA_UI__BASE__STATIC_H

#include "ui/base/window.h"

namespace aspia {

class UiStatic : public UiWindow
{
public:
    UiStatic() = default;
    UiStatic(HWND hwnd);

    bool Create(HWND parent, DWORD style);
    void SetIcon(HICON icon);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__STATIC_H
