/*
* PROJECT:         Aspia Remote Desktop
* FILE:            gui/main_dialog.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "stdafx.h"
#include "main_dialog.h"

LRESULT CMainDialog::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    CenterWindow();
    return 0;
}

LRESULT CMainDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&)
{
    EndDialog(0);
    return 0;
}

LRESULT CMainDialog::OnStartServer(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    static bool started = false;

    CString status;
    CString button;

    if (!started)
    {
        status.LoadStringW(IDS_SERVER_STARTED);
        button.LoadStringW(IDS_STOP);
    }
    else
    {
        status.LoadStringW(IDS_SERVER_STOPPED);
        button.LoadStringW(IDS_START);
    }

    started = !started;

    GetDlgItem(IDC_SERVER_STATUS).SetWindowTextW(status);
    GetDlgItem(IDC_START_SERVER).SetWindowTextW(button);

    return 0;
}

LRESULT CMainDialog::OnConnectToServer(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    return 0;
}
