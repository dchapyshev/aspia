//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/edit.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__EDIT_H
#define _ASPIA_UI__BASE__EDIT_H

#include "ui/base/window.h"

namespace aspia {

class UiEdit : public UiWindow
{
public:
    UiEdit() = default;
    UiEdit(HWND hwnd);

    bool Create(HWND parent, DWORD style);

    void AppendText(const std::wstring& text);
    std::wstring GetText();

    void SetLimitText(int num_characters);
    void SetReadOnly(bool read_only);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__EDIT_H
