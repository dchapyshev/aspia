//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/static.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/static.h"

#include <commctrl.h>
#include <windowsx.h>

namespace aspia {

UiStatic::UiStatic(HWND hwnd)
{
    Attach(hwnd);
}

bool UiStatic::Create(HWND parent, DWORD style)
{
    HINSTANCE instance =
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!instance)
        return false;

    Attach(CreateWindowExW(0,
                           WC_STATICW,
                           L"",
                           WS_CHILD | WS_VISIBLE | style,
                           0, 0, 200, 20,
                           parent,
                           nullptr,
                           instance,
                           nullptr));
    if (hwnd() == nullptr)
        return false;

    return true;
}

void UiStatic::SetIcon(HICON icon)
{
    Static_SetIcon(hwnd(), icon);
}

} // namespace aspia
