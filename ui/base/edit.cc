//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/edit.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/edit.h"

#include <commctrl.h>
#include <windowsx.h>

namespace aspia {

UiEdit::UiEdit(HWND hwnd)
{
    Attach(hwnd);
}

bool UiEdit::Create(HWND parent, DWORD style)
{
    HINSTANCE instance =
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!instance)
        return false;

    Attach(CreateWindowExW(WS_EX_CLIENTEDGE,
                           WC_EDITW,
                           L"",
                           style,
                           0, 0, 0, 0,
                           parent,
                           nullptr,
                           instance,
                           nullptr));

    if (hwnd())
        return true;

    return false;
}

void UiEdit::AppendText(const std::wstring& text)
{
    int length = GetWindowTextLengthW(hwnd());

    Edit_SetSel(hwnd(), length, length);
    Edit_ScrollCaret(hwnd());
    Edit_ReplaceSel(hwnd(), text.c_str());
}

std::wstring UiEdit::GetText()
{
    // Returns the length without null-character.
    int length = GetWindowTextLengthW(hwnd());
    if (length > 0)
    {
        std::wstring string;
        string.resize(length);

        if (GetWindowTextW(hwnd(), &string[0], length + 1))
             return string;
    }

    return std::wstring();
}

} // namespace aspia
