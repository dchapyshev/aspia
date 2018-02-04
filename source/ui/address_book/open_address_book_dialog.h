//
// PROJECT:         Aspia
// FILE:            ui/address_book/open_address_book_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__ADDRESS_BOOK__OPEN_ADDRESS_BOOK_DIALOG_H
#define _ASPIA_UI__ADDRESS_BOOK__OPEN_ADDRESS_BOOK_DIALOG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>

#include "crypto/secure_memory.h"
#include "proto/address_book.pb.h"
#include "ui/resource.h"

namespace aspia {

class OpenAddressBookDialog :
    public CDialogImpl<OpenAddressBookDialog>
{
public:
    enum { IDD = IDD_OPEN_ADDRESS_BOOK };

    OpenAddressBookDialog() = default;

    const std::string& GetPassword() const;

private:
    BEGIN_MSG_MAP(OpenAddressBookDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    LRESULT OnOkButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    SecureString<std::string> password_;
};

} // namespace aspia

#endif // _ASPIA_UI__ADDRESS_BOOK__OPEN_ADDRESS_BOOK_DIALOG_H
