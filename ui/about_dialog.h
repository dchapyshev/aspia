//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/about_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__ABOUT_DIALOG_H
#define _ASPIA_UI__ABOUT_DIALOG_H

#include "ui/base/modal_dialog.h"

namespace aspia {

class UiAboutDialog : public UiModalDialog
{
public:
    UiAboutDialog() = default;

    INT_PTR DoModal(HWND parent) override;

private:
    INT_PTR OnMessage(UINT msg, WPARAM wparam, LPARAM lparam) override;

    ScopedHICON icon_;

    DISALLOW_COPY_AND_ASSIGN(UiAboutDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__ABOUT_DIALOG_H
