//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/power_manage_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__POWER_MANAGE_DIALOG_H
#define _ASPIA_UI__POWER_MANAGE_DIALOG_H

#include "ui/base/modal_dialog.h"

namespace aspia {

class PowerManageDialog : public ModalDialog
{
public:
    PowerManageDialog() = default;

    INT_PTR DoModal(HWND parent) override;

private:
    INT_PTR OnMessage(UINT msg, WPARAM wparam, LPARAM lparam) override;

    DISALLOW_COPY_AND_ASSIGN(PowerManageDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__POWER_MANAGE_DIALOG_H
