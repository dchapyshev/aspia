//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/toolbar.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__TOOLBAR_H
#define _ASPIA_UI__BASE__TOOLBAR_H

#include "ui/base/window.h"

#include <commctrl.h>
#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class UiToolBar : public UiWindow
{
public:
    UiToolBar() = default;
    UiToolBar(HWND hwnd);
    ~UiToolBar() = default;

    bool Create(HWND parent, DWORD style);
    bool ModifyExtendedStyle(DWORD remove, DWORD add);
    void ButtonStructSize(size_t size);
    void AddButtons(UINT number_buttons, TBBUTTON* buttons);
    void InsertButton(int button_index, int command_id, int icon_index, int style);
    void InsertSeparator(int button_index);
    void DeleteButton(int button_index);
    int CommandIdToIndex(int command_id);
    void SetImageList(HIMAGELIST imagelist);
    void SetButtonText(int command_id, const WCHAR* text);
    void SetButtonText(int command_id, const std::wstring& text);
    void SetButtonState(int command_id, BYTE state);
    bool IsButtonChecked(int command_id);
    void CheckButton(int command_id, bool check);
    bool IsButtonEnabled(int command_id);
    void EnableButton(int command_id, bool enable);
    bool IsButtonHidden(int command_id);
    void HideButton(int command_id, bool hide);
    void AutoSize();
    void GetRect(int command_id, RECT* rect);
    void SetPadding(int cx, int cy);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__TOOLBAR_H
