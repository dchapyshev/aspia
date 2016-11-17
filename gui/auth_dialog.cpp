/*
* PROJECT:         Aspia Remote Desktop
* FILE:            gui/auth_dialog.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "gui/auth_dialog.h"

LRESULT CAuthDialog::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    CenterWindow();
    return 0;
}

LRESULT CAuthDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&)
{
    EndDialog(0);
    return 0;
}
