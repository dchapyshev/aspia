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

void UiToolBar::InsertButton(int button_index,
                             int command_id,
                             int icon_index,
                             int style)
{
    TBBUTTON button = { 0 };

    button.idCommand = command_id;
    button.iBitmap   = icon_index;
    button.fsStyle   = style;
    button.fsState   = TBSTATE_ENABLED;

    SendMessageW(hwnd(),
                 TB_INSERTBUTTON,
                 button_index,
                 reinterpret_cast<LPARAM>(&button));
}

void UiToolBar::InsertSeparator(int button_index)
{
    InsertButton(button_index, 0, -1, BTNS_SEP);
}

void UiToolBar::DeleteButton(int button_index)
{
    SendMessageW(hwnd(), TB_DELETEBUTTON, button_index, 0);
}

int UiToolBar::CommandIdToIndex(int command_id)
{
    return SendMessageW(hwnd(), TB_COMMANDTOINDEX, command_id, 0);
}

void UiToolBar::SetImageList(HIMAGELIST imagelist)
{
    SendMessageW(hwnd(),
                 TB_SETIMAGELIST,
                 0,
                 reinterpret_cast<LPARAM>(imagelist));
}

void UiToolBar::SetButtonText(int command_id, const std::wstring& text)
{
    int button_index = CommandIdToIndex(command_id);
    if (button_index == -1)
        return;

    TBBUTTON button = { 0 };

    if (!SendMessageW(hwnd(),
                      TB_GETBUTTON,
                      button_index,
                      reinterpret_cast<LPARAM>(&button)))
    {
        return;
    }

    int string_id = SendMessageW(hwnd(),
                                 TB_ADDSTRING,
                                 0,
                                 reinterpret_cast<LPARAM>(text.c_str()));
    if (string_id == -1)
        return;

    button.iString = string_id;

    DeleteButton(button_index);

    SendMessageW(hwnd(),
                 TB_INSERTBUTTON,
                 button_index,
                 reinterpret_cast<LPARAM>(&button));
}

void UiToolBar::SetButtonState(int command_id, BYTE state)
{
    TBBUTTONINFOW button_info;

    button_info.cbSize  = sizeof(button_info);
    button_info.dwMask  = TBIF_STATE;
    button_info.fsState = state;

    SendMessageW(hwnd(),
                 TB_SETBUTTONINFOW,
                 command_id,
                 reinterpret_cast<LPARAM>(&button_info));
}

bool UiToolBar::IsButtonChecked(int command_id)
{
    return !!SendMessageW(hwnd(), TB_ISBUTTONCHECKED, command_id, 0);
}

void UiToolBar::CheckButton(int command_id, bool check)
{
    SendMessageW(hwnd(), TB_CHECKBUTTON, command_id, MAKELPARAM(check, 0));
}

bool UiToolBar::IsButtonEnabled(int command_id)
{
    return !!SendMessageW(hwnd(), TB_ISBUTTONENABLED, command_id, 0);
}

void UiToolBar::EnableButton(int command_id, bool enable)
{
    SendMessageW(hwnd(), TB_ENABLEBUTTON, command_id, MAKELONG(enable, 0));
}

bool UiToolBar::IsButtonHidden(int command_id)
{
    return !!SendMessageW(hwnd(), TB_ISBUTTONHIDDEN, command_id, 0);
}

void UiToolBar::HideButton(int command_id, bool hide)
{
    SendMessageW(hwnd(), TB_HIDEBUTTON, command_id, MAKELONG(hide, 0));
}

void UiToolBar::AutoSize()
{
    SendMessageW(hwnd(), TB_AUTOSIZE, 0, 0);
}

void UiToolBar::GetRect(int command_id, RECT& rect)
{
    SendMessageW(hwnd(), TB_GETRECT, command_id, reinterpret_cast<LPARAM>(&rect));
}

void UiToolBar::SetPadding(int cx, int cy)
{
    SendMessageW(hwnd(), TB_SETPADDING, 0, MAKELONG(cx, cy));
}

} // namespace aspia
