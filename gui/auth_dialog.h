/*
* PROJECT:         Aspia Remote Desktop
* FILE:            gui/auth_dialog.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_AUTH_DIALOG_H
#define _ASPIA_AUTH_DIALOG_H

#include "resource.h"

class CAuthDialog : public CDialogImpl < CAuthDialog >
{
public:
    enum { IDD = IDD_AUTHDIALOG };

    BEGIN_MSG_MAP(CAuthDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
    END_MSG_MAP()

private:
    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
    LRESULT OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
};

#endif // _ASPIA_AUTH_DIALOG_H
