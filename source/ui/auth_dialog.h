//
// PROJECT:         Aspia
// FILE:            ui/auth_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__AUTH_DIALOG_H
#define _ASPIA_UI__AUTH_DIALOG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <string>

#include "crypto/secure_memory.h"
#include "proto/computer.pb.h"
#include "ui/resource.h"

namespace aspia {

class AuthDialog : public CDialogImpl<AuthDialog>
{
public:
    enum { IDD = IDD_AUTH };

    AuthDialog(const proto::Computer& computer);

    const std::wstring& UserName() const;
    const std::wstring& Password() const;

private:
    BEGIN_MSG_MAP(AuthDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnOkButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

private:
    std::wstring username_;
    SecureString<std::wstring> password_;

    DISALLOW_COPY_AND_ASSIGN(AuthDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__AUTH_DIALOG_H
