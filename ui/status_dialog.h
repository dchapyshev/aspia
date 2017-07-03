//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/status_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__STATUS_DIALOG_H
#define _ASPIA_UI__STATUS_DIALOG_H

#include "base/macros.h"
#include "proto/status.pb.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlframe.h>
#include <atlmisc.h>

namespace aspia {

class UiStatusDialog :
    public CDialogImpl<UiStatusDialog>,
    public CDialogResize<UiStatusDialog>
{
public:
    enum { IDD = IDD_STATUS };

    class Delegate
    {
    public:
        virtual void OnStatusDialogOpen() = 0;
    };

    UiStatusDialog(Delegate* delegate);
    ~UiStatusDialog() = default;

    void SetDestonation(const std::wstring& address, uint16_t port);
    void SetStatus(proto::Status status);

private:
    BEGIN_MSG_MAP(UiStatusDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDCANCEL, OnCloseButton)

        CHAIN_MSG_MAP(CDialogResize<UiStatusDialog>)
    END_MSG_MAP()

    BEGIN_DLGRESIZE_MAP(UiStatusDialog)
        DLGRESIZE_CONTROL(IDC_STATUS_EDIT, DLSZ_SIZE_X | DLSZ_SIZE_Y)
        DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
    END_DLGRESIZE_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCloseButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    void AddMessage(const CString& message);

    Delegate* delegate_ = nullptr;

    CIcon small_icon_;
    CIcon big_icon_;

    DISALLOW_COPY_AND_ASSIGN(UiStatusDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__STATUS_DIALOG_H
