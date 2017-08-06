//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/power_session_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__POWER_SESSION_DIALOG_H
#define _ASPIA_UI__POWER_SESSION_DIALOG_H

#include "base/macros.h"
#include "proto/power_session.pb.h"
#include "ui/base/timer.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>

namespace aspia {

class UiPowerSessionDialog : public CDialogImpl<UiPowerSessionDialog>
{
public:
    enum { IDD = IDD_POWER_HOST };
    enum class Result { CANCEL, EXECUTE };

    UiPowerSessionDialog(proto::PowerEvent::Action action);
    ~UiPowerSessionDialog() = default;

private:
    BEGIN_MSG_MAP(UiPowerSessionDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)

        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnTimer(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnOkButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    void UpdateTimer();
    void Exit(Result result);

    const proto::PowerEvent::Action action_;
    int time_left_;
    CTimer timer_;

    CIcon small_icon_;
    CIcon big_icon_;
    CIcon power_icon_;

    DISALLOW_COPY_AND_ASSIGN(UiPowerSessionDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__POWER_SESSION_DIALOG_H
