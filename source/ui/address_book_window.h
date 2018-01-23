//
// PROJECT:         Aspia
// FILE:            ui/address_book_window.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__ADDRESS_BOOK_WINDOW_H
#define _ASPIA_UI__ADDRESS_BOOK_WINDOW_H

#include "base/message_loop/message_loop.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class AddressBookWindow
    : public CWindowImpl<AddressBookWindow, CWindow, CFrameWinTraits>,
      public MessageLoop::Dispatcher
{
public:
    AddressBookWindow() = default;
    ~AddressBookWindow() = default;

private:
    // MessageLoop::Dispatcher implementation.
    bool Dispatch(const NativeEvent& event) override;

    BEGIN_MSG_MAP(AddressBookWindow)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    END_MSG_MAP()

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnGetMinMaxInfo(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    CIcon small_icon_;
    CIcon big_icon_;
};

} // namespace aspia

#endif // _ASPIA_UI__ADDRESS_BOOK_WINDOW_H
