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

namespace aspia {

class UiToolBar : public UiWindow
{
public:
    UiToolBar() = default;
    UiToolBar(HWND hwnd);
    ~UiToolBar() = default;

    bool Create(HWND parent, DWORD style, HINSTANCE instance);
    bool ModifyExtendedStyle(DWORD remove, DWORD add);
    void ButtonStructSize(size_t size);
    void AddButtons(UINT number_buttons, TBBUTTON* buttons);
    void SetImageList(HIMAGELIST imagelist);
    void SetButtonText(int button_id, const std::wstring& text);
    void SetButtonState(int button_id, BYTE state);
    bool IsButtonChecked(int button_id);
    void CheckButton(int button_id, bool check);
    void AutoSize();
    void GetRect(int button_id, RECT& rect);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__TOOLBAR_H
