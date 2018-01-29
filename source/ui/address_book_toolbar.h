//
// PROJECT:         Aspia
// FILE:            ui/address_book_toolbar.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__ADDRESS_BOOK_TOOLBAR_H
#define _ASPIA_UI__ADDRESS_BOOK_TOOLBAR_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class AddressBookToolbar : public CWindowImpl<AddressBookToolbar, CToolBarCtrl>
{
public:
    AddressBookToolbar() = default;

    bool Create(HWND parent);

private:
    BEGIN_MSG_MAP(AddressBookToolbar)
        NOTIFY_CODE_HANDLER(TTN_GETDISPINFOW, OnGetDispInfo)
    END_MSG_MAP()

    LRESULT OnGetDispInfo(int control_id, LPNMHDR hdr, BOOL& handled);

    CImageListManaged imagelist_;
};

} // namespace aspia

#endif // _ASPIA_UI__ADDRESS_BOOK_TOOLBAR_H
