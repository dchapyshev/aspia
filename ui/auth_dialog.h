//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/auth_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__AUTH_DIALOG_H
#define _ASPIA_UI__AUTH_DIALOG_H

#include "crypto/secure_string.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <string>

namespace aspia {

class UiAuthDialog : public CDialogImpl<UiAuthDialog>
{
public:
    enum { IDD = IDD_AUTH };

    UiAuthDialog() = default;
    ~UiAuthDialog() = default;

    const std::string& UserName() const;
    const std::string& Password() const;

private:
    BEGIN_MSG_MAP(UiAuthDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDC_DONATE_BUTTON, OnCancelButton)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnOkButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

private:
    SecureString<std::string> username_;
    SecureString<std::string> password_;

    DISALLOW_COPY_AND_ASSIGN(UiAuthDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__AUTH_DIALOG_H
