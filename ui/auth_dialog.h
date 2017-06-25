//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/auth_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__AUTH_DIALOG_H
#define _ASPIA_UI__AUTH_DIALOG_H

#include "ui/base/modal_dialog.h"
#include "crypto/secure_string.h"

namespace aspia {

class UiAuthDialog : public UiModalDialog
{
public:
    UiAuthDialog() = default;
    ~UiAuthDialog() = default;

    INT_PTR DoModal(HWND parent) override;

    const std::string& UserName() const;
    const std::string& Password() const;

private:
    INT_PTR OnMessage(UINT msg, WPARAM wparam, LPARAM lparam) override;

    void OnInitDialog();
    void OnClose();
    void OnOkButton();
    void OnCancelButton();

private:
    SecureString<std::string> username_;
    SecureString<std::string> password_;

    DISALLOW_COPY_AND_ASSIGN(UiAuthDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__AUTH_DIALOG_H
