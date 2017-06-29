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

bool UiToolBar::Create(HWND parent, DWORD style)
{
    HINSTANCE instance =
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!instance)
        return false;

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
        static_cast<DWORD>(SendMessageW(TB_GETEXTENDEDSTYLE, 0, 0));

    DWORD new_style = (style & ~remove) | add;

    if (style == new_style)
        return false;

    SendMessageW(TB_SETEXTENDEDSTYLE, 0, new_style);

    return true;
}

void UiToolBar::ButtonStructSize(size_t size)
{
    SendMessageW(TB_BUTTONSTRUCTSIZE, size, 0);
}

void UiToolBar::AddButtons(UINT number_buttons, TBBUTTON* buttons)
{
    SendMessageW(TB_ADDBUTTONS,
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

    SendMessageW(TB_INSERTBUTTON,
                 button_index,
                 reinterpret_cast<LPARAM>(&button));
}

void UiToolBar::InsertSeparator(int button_index)
{
    InsertButton(button_index, 0, -1, BTNS_SEP);
}

void UiToolBar::DeleteButton(int button_index)
{
    SendMessageW(TB_DELETEBUTTON, button_index, 0);
}

int UiToolBar::CommandIdToIndex(int command_id)
{
    return SendMessageW(TB_COMMANDTOINDEX, command_id, 0);
}

void UiToolBar::SetImageList(HIMAGELIST imagelist)
{
    SendMessageW(TB_SETIMAGELIST,
                 0,
                 reinterpret_cast<LPARAM>(imagelist));
}

void UiToolBar::SetButtonText(int command_id, const std::wstring& text)
{
    int button_index = CommandIdToIndex(command_id);
    if (button_index == -1)
        return;

    TBBUTTON button = { 0 };

    if (!SendMessageW(TB_GETBUTTON,
                      button_index,
                      reinterpret_cast<LPARAM>(&button)))
    {
        return;
    }

    int string_id = SendMessageW(TB_ADDSTRING,
                                 0,
                                 reinterpret_cast<LPARAM>(text.c_str()));
    if (string_id == -1)
        return;

    button.iString = string_id;

    DeleteButton(button_index);

    SendMessageW(TB_INSERTBUTTON,
                 button_index,
                 reinterpret_cast<LPARAM>(&button));
}

void UiToolBar::SetButtonState(int command_id, BYTE state)
{
    TBBUTTONINFOW button_info;

    button_info.cbSize  = sizeof(button_info);
    button_info.dwMask  = TBIF_STATE;
    button_info.fsState = state;

    SendMessageW(TB_SETBUTTONINFOW,
                 command_id,
                 reinterpret_cast<LPARAM>(&button_info));
}

bool UiToolBar::IsButtonChecked(int command_id)
{
    return !!SendMessageW(TB_ISBUTTONCHECKED, command_id, 0);
}

void UiToolBar::CheckButton(int command_id, bool check)
{
    SendMessageW(TB_CHECKBUTTON, command_id, MAKELPARAM(check, 0));
}

bool UiToolBar::IsButtonEnabled(int command_id)
{
    return !!SendMessageW(TB_ISBUTTONENABLED, command_id, 0);
}

void UiToolBar::EnableButton(int command_id, bool enable)
{
    SendMessageW(TB_ENABLEBUTTON, command_id, MAKELONG(enable, 0));
}

bool UiToolBar::IsButtonHidden(int command_id)
{
    return !!SendMessageW(TB_ISBUTTONHIDDEN, command_id, 0);
}

void UiToolBar::HideButton(int command_id, bool hide)
{
    SendMessageW(TB_HIDEBUTTON, command_id, MAKELONG(hide, 0));
}

void UiToolBar::AutoSize()
{
    SendMessageW(TB_AUTOSIZE, 0, 0);
}

void UiToolBar::GetRect(int command_id, UiRect& rect)
{
    SendMessageW(TB_GETRECT,
                 command_id,
                 reinterpret_cast<LPARAM>(rect.Pointer()));
}

void UiToolBar::SetPadding(int cx, int cy)
{
    SendMessageW(TB_SETPADDING, 0, MAKELONG(cx, cy));
}

} // namespace aspia
