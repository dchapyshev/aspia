//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/auth_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__AUTH_DIALOG_H
#define _ASPIA_UI__AUTH_DIALOG_H

#include <string>

#include "ui/base/modal_dialog.h"

namespace aspia {

class AuthDialog : public ModalDialog
{
public:
    AuthDialog() = default;
    ~AuthDialog();

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
    std::string username_;
    std::string password_;

    DISALLOW_COPY_AND_ASSIGN(AuthDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__AUTH_DIALOG_H
