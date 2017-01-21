//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/auth_dialog.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "gui/auth_dialog.h"

namespace aspia {

BOOL CAuthDialog::OnInitDialog(CWindow focus_window, LPARAM lParam)
{
    CenterWindow();
    return TRUE;
}

void CAuthDialog::OnClose()
{
    EndDialog(0);
}

} // namespace aspia
