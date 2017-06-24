//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/toolbar.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/toolbar.h"

namespace aspia {

UiToolBar::UiToolBar(HWND hwnd) :
    UiWindow(hwnd)
{
    // Nothing
}

bool UiToolBar::Create(HWND parent, DWORD style, HINSTANCE instance)
{
    Attach(CreateWindowExW(0,
                           TOOLBARCLASSNAMEW,
                           nullptr,
                           WS_CHILD | WS_VISIBLE | style,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           parent,
                           nullptr,
                           instance,
                           nullptr));
    if (hwnd() == nullptr)
        return false;

    return true;
}

bool UiToolBar::ModifyExtendedStyle(DWORD remove, DWORD add)
{
    DWORD style =
        static_cast<DWORD>(SendMessageW(hwnd(), TB_GETEXTENDEDSTYLE, 0, 0));

    DWORD new_style = (style & ~remove) | add;

    if (style == new_style)
        return false;

    SendMessageW(hwnd(), TB_SETEXTENDEDSTYLE, 0, new_style);

    return true;
}

void UiToolBar::ButtonStructSize(size_t size)
{
    SendMessageW(hwnd(), TB_BUTTONSTRUCTSIZE, size, 0);
}

void UiToolBar::AddButtons(UINT number_buttons, TBBUTTON* buttons)
{
    SendMessageW(hwnd(),
                 TB_ADDBUTTONS,
                 number_buttons,
                 reinterpret_cast<LPARAM>(buttons));
}

void UiToolBar::SetImageList(HIMAGELIST imagelist)
{
    SendMessageW(hwnd(),
                 TB_SETIMAGELIST,
                 0,
                 reinterpret_cast<LPARAM>(imagelist));
}

void UiToolBar::SetButtonText(int button_id, const std::wstring& text)
{
    TBBUTTONINFOW button = { 0 };

    button.cbSize  = sizeof(button);
    button.dwMask  = TBIF_TEXT;
    button.pszText = const_cast<LPWSTR>(text.c_str());

    SendMessageW(hwnd(),
                 TB_SETBUTTONINFO,
                 button_id,
                 reinterpret_cast<LPARAM>(&button));
}

void UiToolBar::SetButtonState(int button_id, BYTE state)
{
    TBBUTTONINFOW button_info;

    button_info.cbSize  = sizeof(button_info);
    button_info.dwMask  = TBIF_STATE;
    button_info.fsState = state;

    SendMessageW(hwnd(),
                 TB_SETBUTTONINFOW,
                 button_id,
                 reinterpret_cast<LPARAM>(&button_info));
}

bool UiToolBar::IsButtonChecked(int button_id)
{
    return !!SendMessageW(hwnd(), TB_ISBUTTONCHECKED, button_id, 0);
}

void UiToolBar::CheckButton(int button_id, bool check)
{
    SendMessageW(hwnd(), TB_CHECKBUTTON, button_id, MAKELPARAM(check, 0));
}

void UiToolBar::AutoSize()
{
    SendMessageW(hwnd(), TB_AUTOSIZE, 0, 0);
}

void UiToolBar::GetRect(int button_id, RECT& rect)
{
    SendMessageW(hwnd(), TB_GETRECT, button_id, reinterpret_cast<LPARAM>(&rect));
}

} // namespace aspia
