//
// PROJECT:         Aspia
// FILE:            ui/address_book/address_book_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__ADDRESS_BOOK__ADDRESS_BOOK_DIALOG_H
#define _ASPIA_UI__ADDRESS_BOOK__ADDRESS_BOOK_DIALOG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlframe.h>

#include "proto/address_book.pb.h"
#include "ui/resource.h"

namespace aspia {

class AddressBookDialog :
    public CDialogImpl<AddressBookDialog>,
    public CDialogResize<AddressBookDialog>
{
public:
    enum { IDD = IDD_ADDRESS_BOOK };

    AddressBookDialog() = default;
    AddressBookDialog(const proto::AddressBook& address_book,
                      const proto::ComputerGroup& root_group);
    ~AddressBookDialog();

    const proto::AddressBook& GetAddressBook() const;
    const proto::ComputerGroup& GetRootComputerGroup() const;

private:
    BEGIN_MSG_MAP(AddressBookDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)

        COMMAND_HANDLER(IDC_ENCRYPTION_TYPE_COMBO, CBN_SELCHANGE, OnEncryptionTypeChanged)

        CHAIN_MSG_MAP(CDialogResize<AddressBookDialog>)
    END_MSG_MAP()

    BEGIN_DLGRESIZE_MAP(AddressBookDialog)
        DLGRESIZE_CONTROL(IDC_NAME_EDIT, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_ENCRYPTION_TYPE_COMBO, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_PASSWORD_EDIT, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_PASSWORD_RETRY_EDIT, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_COMMENT_EDIT, DLSZ_SIZE_X | DLSZ_SIZE_Y)
        DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
    END_DLGRESIZE_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    LRESULT OnOkButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    LRESULT OnEncryptionTypeChanged(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    proto::AddressBook address_book_;
    proto::ComputerGroup root_group_;
};

} // namespace aspia

#endif // _ASPIA_UI__ADDRESS_BOOK__ADDRESS_BOOK_DIALOG_H
