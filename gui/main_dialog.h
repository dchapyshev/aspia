/*
* PROJECT:         Aspia Remote Desktop
* FILE:            gui/main_dialog.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_MAIN_DIALOG_H
#define _ASPIA_MAIN_DIALOG_H

#include "resource.h"

class CMainDialog : public CDialogImpl < CMainDialog >
{
public:
    enum { IDD = IDD_MAINDIALOG };

    BEGIN_MSG_MAP(CMainDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDC_START_SERVER, OnStartServer)
        COMMAND_ID_HANDLER(IDC_CONNECT, OnConnectToServer)
    END_MSG_MAP()

private:
    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
    LRESULT OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);

    LRESULT OnStartServer(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
    LRESULT OnConnectToServer(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
};

#endif // _ASPIA_MAIN_DIALOG_H
